/**
 * captain.cpp
 * Author: Lijing Wang
 * Date: 5/5/2015
 */

#include "commo.hpp"
#include "captain.hpp"
#include <iostream>
#include <chrono>
#include <unistd.h>

namespace ndnpaxos {

ndn::Name Captain::dumName_ = ndn::Name("");

// no thread pool
Captain::Captain(View &view, callback_t& callback)
  : view_(&view), 
    max_chosen_(0), max_chosen_without_hole_(0), max_slot_(0), last_slot_(0),
    value_id_(view_->whoami()), window_size_(1),
    callback_(callback), callback_full_(NULL), callback_latency_(NULL), 
    work_(true) {

//  commo_ = new Commo(this, view);
  chosen_values_.push_back(NULL);
  acceptors_.push_back(NULL);
  ballot_id_t max_id = (1 << 16) + view_->master_id();
  LOG_INFO_CAP("init acceptors for %d length and update max_proposed_value == %d", view_->length(), max_id);
  for(int i = 1; i <= view_->length(); i++) {
    acceptors_.emplace_back(new Acceptor(*view_));
    acceptors_[i]->update_max_proposed(max_id);
  }
}

// now using this 
Captain::Captain(View &view, int window_size)
  : view_(&view), 
    max_chosen_(0), max_chosen_without_hole_(0), max_slot_(0), last_slot_(0),
    value_id_(view_->whoami()), window_size_(window_size),
    callback_(NULL), callback_full_(NULL), callback_latency_(NULL), 
    work_(true) {

//  commo_ = new Commo(this, view);
  chosen_values_.push_back(NULL);
  acceptors_.push_back(NULL);

  ballot_id_t max_id = (1 << 16) + view_->master_id();
  LOG_INFO_CAP("init acceptors for %d length and update max_proposed_value == %d", view_->length(), max_id);
  for(int i = 1; i <= view_->length(); i++) {
    acceptors_.emplace_back(new Acceptor(*view_));
    acceptors_[i]->update_max_proposed(max_id);
  }
}


Captain::~Captain() {
}

void Captain::set_callback(callback_t& cb) { 
  callback_ = cb;
}

void Captain::set_callback(callback_full_t& cb) { 
  callback_full_ = cb;
}

void Captain::set_callback(callback_latency_t& cb) { 
  callback_latency_ = cb;
}

/** 
 * return node_id
 */
node_id_t Captain::get_node_id() {
  return view_->whoami();
}

/**
 * set commo_handler 
 */
void Captain::set_commo(Commo *commo) {
  commo_ = commo;
}

/** 
 * client commits one value to captain
 * TODO need to change by window_size
 */
void Captain::commit_recover() {
}

void Captain::master_change(PropValue* prop_value) {
  if(view_->if_master()) {
    commit(prop_value);
  } else {
    LOG_INFO_CAP("the original master:%u does not respond, start changing to:%u", view_->master_id(), view_->whoami());
    PropValue *prop_cmd = new PropValue();
    prop_cmd->set_cmd_type(SET_MASTER);
    value_id_mutex_.lock();
    value_id_ += (1 << 16);
    value_id_t id = value_id_;
    prop_cmd->set_id(value_id_);
    value_id_mutex_.unlock();

    std::string command = "set_master Time_" + std::to_string(id >> 16) + " from " + view_->hostname();
    prop_cmd->set_data(command);
    proposers_mutex_.lock();
    tocommit_values_.push(prop_cmd);
    proposers_mutex_.unlock();
    commit(prop_value);
  }
}

void Captain::commit(std::string& data, ndn::Name& dataName) {

  PropValue *prop_value = new PropValue();
  prop_value->set_data(data);
  value_id_mutex_.lock();
  value_id_ += (1 << 16);
  value_id_t id = value_id_;
  prop_value->set_id(value_id_);
  value_id_mutex_.unlock();

  if (view_->if_master()) {
    commit(prop_value, dataName);
  } else {
#if MODE_TYPE == 1
    node_id_t old_master = view_->master_id();
    // change quorums n_quorums
    master_mutex_.lock();
    view_->print_quorums();
    view_->print_n_quorums();
    view_->set_master(view_->whoami());
    view_->remove_quorums(old_master);
    node_id_t old_nq = *(view_->get_n_quorums()->begin());
    view_->add_quorums(old_nq);
    view_->remove_n_quorums(old_nq);
    view_->print_quorums();
    view_->print_n_quorums();
    master_mutex_.unlock();

    MsgCommand *msg_cmd = msg_command(SET_QUORUM);
    commo_->send_one_msg(msg_cmd, COMMAND, old_nq);

    LOG_INFO_CAP("master_id changed from %u to %u", old_master, view_->whoami());
    commit(prop_value, dataName);
#else
    MsgCommit *msg_com = msg_commit(prop_value);
    // blocking style receive reply or timeout now no body call setFilter or run()
    commo_->send_one_msg(msg_com, COMMIT, view_->master_id());
    boost::mutex::scoped_lock lock(commit_ok_mutex_);
    commit_ok_ = false; 
    while (!commit_ok_) commit_ok_cond_.wait(lock);
    //std::cout << "I finish waiting!!!!" << std::endl;
#endif
  }

  LOG_DEBUG_CAP("commit over!!!");
}

void Captain::commit(PropValue* prop_value, ndn::Name &dataName) {

  LOG_DEBUG_CAP("<commit_value> Start");


  if (proposers_.size() > window_size_) {
    LOG_INFO_CAP("Error Occur!!!! proposers_.size() %llu > window_size! %llu", proposers_.size(), window_size_);
    return;
  }

  // if all proposers are active, push commit value into waiting queue(tocommit_values)

  proposers_mutex_.lock();


  if (proposers_.size() == window_size_) {
    tocommit_values_.push(prop_value);
    tocommit_clients_.push(dataName);

    proposers_mutex_.unlock();
    LOG_DEBUG_CAP("push into tocommit_values & tocommit_clients queue");
    return;
  } 
  
  // if there exits at least one proposer unactive, but queue has uncommitted values, commit from queue first
  if (tocommit_values_.size() > 0) {
    tocommit_values_.push(prop_value);
    tocommit_clients_.push(dataName);
    prop_value = tocommit_values_.front();
    dataName = tocommit_clients_.front();
    tocommit_values_.pop();
    tocommit_clients_.pop();
  }

  proposer_info_t *prop_info = new proposer_info_t(1, dataName);
  prop_info->curr_proposer = new Proposer(*view_, *prop_value); 

  max_slot_++;
  proposers_[max_slot_] = prop_info;

  proposers_[max_slot_]->curr_proposer->gen_next_ballot();
  proposers_[max_slot_]->curr_proposer->init_curr_value();
  MsgAccept *msg_acc = proposers_[max_slot_]->curr_proposer->msg_accept();
  msg_acc->mutable_msg_header()->set_slot_id(max_slot_);
  proposers_[max_slot_]->proposer_status = PHASEII;

  proposers_mutex_.unlock();

  max_chosen_mutex_.lock();
  if (max_chosen_ > last_slot_ && chosen_values_[last_slot_ + 1]) {
    last_slot_++;
    msg_acc->set_last_slot(last_slot_);
    value_id_t last_value = chosen_values_[last_slot_]->id();
    msg_acc->set_last_value(last_value);
  }
  max_chosen_mutex_.unlock();

  commo_->broadcast_msg(msg_acc, ACCEPT);

}

/**
 * handle message from commo, all kinds of message
 */
//void Captain::handle_msg(google::protobuf::Message *msg, MsgType msg_type) {
////  pool_->enqueue(&Captain::handle_msg_thread, msg, msg_type);
//  pool_->enqueue(std::bind(&Captain::handle_msg_thread, this, msg, msg_type));
//}

/**
 * handle message from commo, all kinds of message
 */
void Captain::handle_msg(google::protobuf::Message *msg, MsgType msg_type, ndn::Name &dataName) {

  work_mutex_.lock();
  if (work_ == false) {
    LOG_DEBUG_CAP("%sI'm DEAD --NodeID %u", BAK_RED, view_->whoami());
    work_mutex_.unlock();
    return;
  }
  work_mutex_.unlock();

  LOG_TRACE_CAP("<handle_msg> Start (msg_type):%d", msg_type);

  switch (msg_type) {

    case PREPARE: {
      // acceptor should handle prepare message
      MsgPrepare *msg_pre = (MsgPrepare *)msg;

      slot_id_t acc_slot = msg_pre->msg_header().slot_id();
      LOG_TRACE_CAP("(msg_type):PREPARE, (slot_id): %llu", acc_slot);
      // IMPORTANT!!! if there is no such acceptor then init

      acceptors_mutex_.lock();
      for (int i = acceptors_.size(); i <= acc_slot; i++) {
        LOG_TRACE_CAP("(msg_type):PREPARE, New Acceptor");
        acceptors_.emplace_back(new Acceptor(*view_));
      }

      MsgAckPrepare * msg_ack_pre = acceptors_[acc_slot]->handle_msg_prepare(msg_pre);
      acceptors_mutex_.unlock();

      if (msg_pre->msg_header().node_id() == view_->whoami())
//        handle_msg(msg_ack_pre, PROMISE);
        commo_->send_one_msg(msg_ack_pre, PROMISE);
      else // receiver should reply to PROMISE
        commo_->send_one_msg(msg_ack_pre, PROMISE, msg_pre->msg_header().node_id(), dataName);

      break;
    }

    case PROMISE: {
      // proposer should handle ack of prepare message

      MsgAckPrepare *msg_ack_pre = (MsgAckPrepare *)msg;

      slot_id_t slot_id = msg_ack_pre->msg_header().slot_id();
      // if we don't have such proposer ,return
      proposers_mutex_.lock();

      if (proposers_.find(slot_id) == proposers_.end()) {
        LOG_TRACE_CAP("(msg_type):PROMISE,proposers don't have this (slot_id):%llu Return!", slot_id); 
        proposers_mutex_.unlock();
        return;
      }

      if (proposers_[slot_id]->proposer_status == INIT) 
        proposers_[slot_id]->proposer_status = PHASEI;
      else if (proposers_[slot_id]->proposer_status != PHASEI) {
        LOG_TRACE_CAP("(msg_type):PROMISE, (proposer_status_):%d NOT in PhaseI Return!", proposers_[slot_id]->proposer_status);
        proposers_mutex_.unlock();
        return;
      }

      switch (proposers_[slot_id]->curr_proposer->handle_msg_promise(msg_ack_pre)) {
        case DROP: {
          proposers_mutex_.unlock();
          break;
        }
        case NOT_ENOUGH: {
          proposers_mutex_.unlock();
          break;
        }        
        case CONTINUE: {
          // Send to all acceptors in view
          LOG_TRACE_CAP("(msg_type):PROMISE, Continue to Phase II");


         MsgAccept *msg_acc = proposers_[slot_id]->curr_proposer->msg_accept();

         msg_acc->mutable_msg_header()->set_slot_id(slot_id);
         // IMPORTANT set status
         proposers_[slot_id]->proposer_status = PHASEII;
        
          
         proposers_mutex_.unlock();  

         commo_->broadcast_msg(msg_acc, ACCEPT);
         break;
        }
        case RESTART: {  //RESTART
          LOG_TRACE_CAP("(msg_type):PROMISE, RESTART");
          MsgPrepare *msg_pre = proposers_[slot_id]->curr_proposer->restart_msg_prepare();

          msg_pre->mutable_msg_header()->set_slot_id(slot_id);
          proposers_[slot_id]->proposer_status = INIT;
          proposers_mutex_.unlock();

          commo_->broadcast_msg(msg_pre, PREPARE);
          break;
        }
        default: {
          proposers_mutex_.unlock();
        }
      }
      break;
    }
                
    case ACCEPT: {
      // acceptor should handle accept message
      MsgAccept *msg_acc = (MsgAccept *)msg;
      slot_id_t acc_slot = msg_acc->msg_header().slot_id();

      LOG_TRACE_CAP("(msg_type):ACCEPT, (slot_id):%llu", acc_slot);
      // IMPORTANT!!! if there is no such acceptor then init

      acceptors_mutex_.lock();
      for (int i = acceptors_.size(); i <= acc_slot; i++) {
        LOG_TRACE_CAP("(msg_type):PREPARE, New Acceptor");
        acceptors_.emplace_back(new Acceptor(*view_));
      }

      MsgAckAccept *msg_ack_acc = acceptors_[acc_slot]->handle_msg_accept(msg_acc);
      acceptors_mutex_.unlock();

      if (msg_acc->msg_header().node_id() == view_->whoami())
//        handle_msg(msg_ack_acc, ACCEPTED);
        commo_->send_one_msg(msg_ack_acc, ACCEPTED);
      else { 

        if (msg_acc->has_last_slot()) {
          slot_id_t dec_slot = msg_acc->last_slot(); 

          if (max_chosen_ >= dec_slot && chosen_values_[dec_slot]) {
            return;
          }

          LOG_DEBUG_CAP("%s(msg_type):ACCEPT - DECIDE (slot_id):%llu from (node_id):%u --NodeID %u handle", 
                        UND_RED, dec_slot, msg_acc->msg_header().node_id(), view_->whoami());

          if (acceptors_.size() > dec_slot && acceptors_[dec_slot]->get_max_value() && 
             (acceptors_[dec_slot]->get_max_value()->id() == msg_acc->last_value())) {
            // the value is stored in acceptors_[dec_slot]->max_value_
            add_learn_value(dec_slot, acceptors_[dec_slot]->get_max_value(), msg_acc->msg_header().node_id()); 
            
            // produce /prefix/log/slot_id/value_id data value_data
            commo_->produce_log(dec_slot, chosen_values_[dec_slot]);

            // NDN to avoid timeout  
          } 
        }
        commo_->send_one_msg(msg_ack_acc, ACCEPTED, msg_acc->msg_header().node_id(), dataName);
      }
        

      break;
    } 

    case ACCEPTED: {
      // proposer should handle ack of accept message

      MsgAckAccept *msg_ack_acc = (MsgAckAccept *)msg; 

      slot_id_t slot_id = msg_ack_acc->msg_header().slot_id();
      // if we don't have such proposer ,return

      proposers_mutex_.lock();

      if (proposers_.find(slot_id) == proposers_.end()) {
        LOG_TRACE_CAP("(msg_type):ACCEPTED,proposers don't have this (slot_id):%llu Return!", slot_id);
        proposers_mutex_.unlock();
        return;
      }

      // handle_msg_accepted

      if (proposers_[slot_id]->proposer_status != PHASEII) {
        LOG_TRACE_CAP("(msg_type):ACCEPTED, (proposer_status_):%d NOT in PhaseII Return!", proposers_[slot_id]->proposer_status);
        proposers_mutex_.unlock();
        return;
      }

      AckType type = proposers_[slot_id]->curr_proposer->handle_msg_accepted(msg_ack_acc);

      switch (type) {
        case DROP: {
          proposers_mutex_.unlock();
          break;
        }

        case NOT_ENOUGH: {
          proposers_mutex_.unlock();
          break;
        }        
        
        case CHOOSE: {

          // First add the chosen_value into chosen_values_ 
          PropValue *chosen_value = proposers_[slot_id]->curr_proposer->get_chosen_value();
          PropValue *init_value = proposers_[slot_id]->curr_proposer->get_init_value();
          int try_time = proposers_[slot_id]->try_time;
          proposers_[slot_id]->proposer_status = CHOSEN;


          LOG_DEBUG_CAP("%sNodeID:%u Successfully Choose (value):%s ! (slot_id):%llu %s", 
                        BAK_MAG, view_->whoami(), chosen_value->data().c_str(), slot_id, NRM);


          proposers_mutex_.unlock();

          if (chosen_value->id() == init_value->id()) {
            LOG_DEBUG_CAP("inform client %s chosen!", chosen_value->data().c_str());
//            std::cout << "dataName " << proposers_[slot_id]->client_name << std::endl;
            commo_->inform_client(slot_id, try_time, proposers_[slot_id]->client_name);
            if (callback_latency_) {
              callback_latency_(slot_id, *chosen_value, try_time);
            }
          }

          // important change max_chosen & max_chosen
          add_chosen_value(slot_id, chosen_value);


          LOG_DEBUG_CAP("(max_chosen_):%llu (max_chosen_without_hole_):%llu (chosen_values.size()):%lu", 
                        max_chosen_, max_chosen_without_hole_, chosen_values_.size());
          LOG_DEBUG_CAP("(msg_type):ACCEPTED, Broadcast this chosen_value");

          proposers_mutex_.lock();

          proposers_.erase(slot_id);
          if (chosen_value->id() == init_value->id()) {

            // start committing a new value from queue
//            tocommit_values_mutex_.lock();
            
            if (tocommit_values_.empty()) {
              LOG_DEBUG_CAP("This proposer END MISSION Temp Node_ID:%u max_chosen_without_hole_:%llu", 
                  view_->whoami(), max_chosen_without_hole_);
//              tocommit_values_mutex_.unlock();
              proposers_mutex_.unlock();


            } else {

              PropValue *prop_value = tocommit_values_.front();
              tocommit_values_.pop();            
              ndn::Name dataName = tocommit_clients_.front();
              tocommit_clients_.pop();
//              tocommit_values_mutex_.unlock();
  
              proposer_info_t *prop_info = new proposer_info_t(1, dataName);
              prop_info->curr_proposer = new Proposer(*view_, *prop_value); 
            
              max_slot_++;
  
              proposers_[max_slot_] = prop_info;

              proposers_[max_slot_]->curr_proposer->gen_next_ballot();
              proposers_[max_slot_]->curr_proposer->init_curr_value();
              MsgAccept *msg_acc = proposers_[max_slot_]->curr_proposer->msg_accept();
              msg_acc->mutable_msg_header()->set_slot_id(max_slot_);
              proposers_[max_slot_]->proposer_status = PHASEII;
            
              proposers_mutex_.unlock();
              LOG_TRACE_CAP("after finish one, commit from queue, broadcast it");

              max_chosen_mutex_.lock();
              if (max_chosen_ > last_slot_ && chosen_values_[last_slot_ + 1]) {
                last_slot_++;
                msg_acc->set_last_slot(last_slot_);
                value_id_t last_value = chosen_values_[last_slot_]->id();
                msg_acc->set_last_value(last_value);
              }
              max_chosen_mutex_.unlock();

              commo_->broadcast_msg(msg_acc, ACCEPT);

            }

//            if (callback_latency_) {
//              callback_latency_(slot_id, *chosen_value, try_time);
//            }


          } else {
            // recommit the same value need to change

            try_time++;

            proposer_info_t *prop_info = new proposer_info_t(try_time, dataName);
            prop_info->curr_proposer = new Proposer(*view_, *init_value); 
          
            max_slot_++;

            proposers_[max_slot_] = prop_info;
            MsgPrepare *msg_pre = proposers_[max_slot_]->curr_proposer->msg_prepare();
            proposers_[max_slot_]->proposer_status = INIT;
            msg_pre->mutable_msg_header()->set_slot_id(max_slot_);

            proposers_mutex_.unlock();

            LOG_INFO_CAP("Recommit the same (value):%s try_time :%d!!!", init_value->data().c_str(), try_time);
            commo_->broadcast_msg(msg_pre, PREPARE);
            
          }

          break;
        }

        default: { //RESTART
          LOG_DEBUG_CAP("--NodeID:%u (msg_type):ACCEPTED, %sRESTART!%s", view_->whoami(), TXT_RED, NRM); 
          MsgPrepare *msg_pre = proposers_[slot_id]->curr_proposer->restart_msg_prepare();

          msg_pre->mutable_msg_header()->set_slot_id(slot_id);
          proposers_[slot_id]->proposer_status = INIT;
          proposers_mutex_.unlock();

          commo_->broadcast_msg(msg_pre, PREPARE);
         
        }
      }
      break;
    }

    case DECIDE: {
      // captain should handle this message
      MsgDecide *msg_dec = (MsgDecide *)msg;
      slot_id_t dec_slot = msg_dec->msg_header().slot_id();

      if (max_chosen_ >= dec_slot && chosen_values_[dec_slot]) {
        return;
      }

      LOG_DEBUG_CAP("%s(msg_type):DECIDE (slot_id):%llu from (node_id):%u --NodeID %u handle", 
                    UND_RED, dec_slot, msg_dec->msg_header().node_id(), view_->whoami());

      if (acceptors_.size() > dec_slot && acceptors_[dec_slot]->get_max_value() && 
         (acceptors_[dec_slot]->get_max_value()->id() == msg_dec->value_id())) {
        // the value is stored in acceptors_[dec_slot]->max_value_
        add_learn_value(dec_slot, acceptors_[dec_slot]->get_max_value(), msg_dec->msg_header().node_id()); 
        // NDN to avoid timeout  
//        MsgCommand *msg_cmd = msg_command(REPLY_DECIDE);
//        commo_->send_one_msg(msg_cmd, COMMAND, msg_dec->msg_header().node_id(), dataName);
      } else {
        // acceptors_[dec_slot] doesn't contain such value, need learn from this sender
        MsgLearn *msg_lea = msg_learn(dec_slot);
        commo_->send_one_msg(msg_lea, LEARN, msg_dec->msg_header().node_id(), dataName);
      } 
      break;
    }

    case LEARN: {
      // captain should handle this message
      MsgLearn *msg_lea = (MsgLearn *)msg;
      slot_id_t lea_slot = msg_lea->msg_header().slot_id();

      LOG_DEBUG_CAP("%s(msg_type):LEARN (slot_id):%llu from (node_id):%u --NodeID %u handle", 
                    UND_GRN, lea_slot, msg_lea->msg_header().node_id(), view_->whoami());

      if (lea_slot > max_chosen_ || chosen_values_[lea_slot] == NULL) {
        return;
      }

      MsgTeach *msg_tea = msg_teach(lea_slot);
      commo_->send_one_msg(msg_tea, TEACH, msg_lea->msg_header().node_id());
      break;
    }

    case TEACH: {
      // captain should handle this message
      MsgTeach *msg_tea = (MsgTeach *)msg;
      slot_id_t tea_slot = msg_tea->msg_header().slot_id();

      // NDN to avoid timeout  
      MsgCommand *msg_cmd = msg_command(REPLY_TEACH);
      commo_->send_one_msg(msg_cmd, COMMAND, msg_tea->msg_header().node_id(), dataName);

      if (max_chosen_ >= tea_slot && chosen_values_[tea_slot]) {
        return;
      }

      LOG_DEBUG_CAP("%s(msg_type):TEACH (slot_id):%llu from (node_id):%u --NodeID %u handle", 
                    UND_YEL, tea_slot, msg_tea->msg_header().node_id(), view_->whoami());
      // only when has value
      add_learn_value(tea_slot, msg_tea->mutable_prop_value(), msg_tea->msg_header().node_id());

      break;
    }

    case COMMIT: {
      // captain should handle this message

      MsgCommit *msg_com = (MsgCommit *)msg;

      if (view_->if_master()) {
        LOG_INFO_CAP("%s(msg_type):COMMIT from (node_id):%u --NodeID %u handle", 
                    UND_YEL, msg_com->msg_header().node_id(), view_->whoami());
        commo_->send_one_msg(msg_com, COMMIT, msg_com->msg_header().node_id(), dataName);
        commit(msg_com->mutable_prop_value());
      } else {
        value_id_t id = msg_com->mutable_prop_value()->id();
        LOG_DEBUG_CAP("%s(msg_type):Reply_COMMIT from (node_id):%u --NodeID %u handle", 
                    UND_YEL, msg_com->msg_header().node_id(), view_->whoami());

        boost::mutex::scoped_lock lock(commit_ok_mutex_);
        commit_ok_ = true;
        commit_ok_cond_.notify_one();
      }


      break;
    }

    case COMMAND: {
      // captain should handle this message
      MsgCommand *msg_cmd = (MsgCommand *)msg;
      LOG_INFO_CAP("%s(msg_type):COMMAND from (node_id):%u --NodeID %u handle", 
                    UND_YEL, msg_cmd->msg_header().node_id(), view_->whoami());

      switch (msg_cmd->cmd_type()) {
        case SET_QUORUM: {
          master_mutex_.lock();
          view_->print_quorums();
          view_->print_n_quorums();
//          view_->remove_quorums(old_master);
          view_->set_master(1);

          view_->add_quorums(view_->whoami());
          view_->remove_n_quorums(view_->whoami());
          view_->print_quorums();
          view_->print_n_quorums();
          master_mutex_.unlock();

          break;
        }
      }

      break;
    }
    default: 
      break;
  }
}

/**
 * Return Msg_header 
 */
MsgHeader *Captain::set_msg_header(MsgType msg_type, slot_id_t slot_id) {
  MsgHeader *msg_header = new MsgHeader();
  msg_header->set_msg_type(msg_type);
  msg_header->set_node_id(view_->whoami());
  msg_header->set_slot_id(slot_id);
  return msg_header;
}

/**
 * Return Decide Message
 */
MsgDecide *Captain::msg_decide(slot_id_t slot_id) {
  MsgHeader *msg_header = set_msg_header(MsgType::DECIDE, slot_id);
  MsgDecide *msg_dec = new MsgDecide();
  msg_dec->set_allocated_msg_header(msg_header); 
//  msg_dec->set_value_id(curr_proposer_->get_chosen_value()->id());
  msg_dec->set_value_id(chosen_values_[slot_id]->id());
  return msg_dec;
}

/**
 * Return Decide Message
 */
MsgDecide *Captain::msg_decide(slot_id_t slot_id, value_id_t value_id) {
  MsgHeader *msg_header = set_msg_header(MsgType::DECIDE, slot_id);
  MsgDecide *msg_dec = new MsgDecide();
  msg_dec->set_allocated_msg_header(msg_header); 
  msg_dec->set_value_id(value_id);
  return msg_dec;
}

/**
 * Return Learn Message
 */
MsgLearn *Captain::msg_learn(slot_id_t slot_id) {
  MsgHeader *msg_header = set_msg_header(MsgType::LEARN, slot_id);
  MsgLearn *msg_lea = new MsgLearn();
  msg_lea->set_allocated_msg_header(msg_header);
  return msg_lea;
}

/**
 * Return Teach Message
 */
MsgTeach *Captain::msg_teach(slot_id_t slot_id) {
  MsgHeader *msg_header = set_msg_header(MsgType::TEACH, slot_id);
  MsgTeach *msg_tea = new MsgTeach();
  msg_tea->set_allocated_msg_header(msg_header);
  msg_tea->set_allocated_prop_value(chosen_values_[slot_id]);
  return msg_tea; 
}

/**
 * Return Commit Message
 */
MsgCommit *Captain::msg_commit(PropValue *prop_value) {
  MsgHeader *msg_header = set_msg_header(MsgType::COMMIT, 0);
  MsgCommit *msg_com = new MsgCommit();
  msg_com->set_allocated_msg_header(msg_header);
  msg_com->set_allocated_prop_value(prop_value);
  return msg_com; 
}

/**
 * Return Command Message
 */
MsgCommand *Captain::msg_command(CmdType cmd_type) {
  MsgHeader *msg_header = set_msg_header(MsgType::COMMAND, 0);
  MsgCommand *msg_cmd = new MsgCommand();
  msg_cmd->set_allocated_msg_header(msg_header);
  msg_cmd->set_cmd_type(cmd_type);
  return msg_cmd; 
}

/** 
 * Callback function after commit_value  
 */
void Captain::clean() {
//  curr_proposer_mutex_.lock();
//  if (curr_proposer_) { 
////    delete curr_proposer_;
//    curr_proposer_ = NULL;
//  }
//  curr_proposer_mutex_.unlock();
}

void Captain::crash() {
  work_mutex_.lock();
  work_ = false;
//  curr_proposer_mutex_.lock();
//  curr_proposer_ = NULL;
//  curr_proposer_mutex_.unlock();
  work_mutex_.unlock();
}

void Captain::recover() {
}

bool Captain::get_status() {
  return work_;
}

void Captain::print_chosen_values() {
  LOG_INFO_CAP("%s%sNodeID:%u (chosen_values_): %s", BAK_BLU, TXT_WHT, view_->whoami(), NRM);
  if (chosen_values_.size() == 1) {
     LOG_INFO_CAP("%sEMPTY!%s", BLD_RED, NRM); 
  }
  for (uint64_t i = 1; i < chosen_values_.size(); i++) {
    if (chosen_values_[i] != NULL) {
      LOG_INFO_CAP("%s%s(slot_id):%llu (value) id:%llu data: %s%s", 
                     BAK_CYN, TXT_WHT, i, chosen_values_[i]->id(), chosen_values_[i]->data().c_str(), NRM);
    } else {
      LOG_INFO_CAP("%s%s(slot_id):%llu (value):NULL%s", BAK_CYN, TXT_WHT, i, NRM); 
    }
  }
}

std::vector<PropValue *> Captain::get_chosen_values() {
  return chosen_values_; 
}

PropValue *Captain::get_chosen_value(slot_id_t slot_id) {
  if (slot_id <= max_chosen_)
    return chosen_values_[slot_id];
  else return NULL;
}

/**
 * Add a new chosen_value 
 */
void Captain::add_chosen_value(slot_id_t slot_id, PropValue *prop_value) {

  max_chosen_mutex_.lock();

  if (slot_id <= max_chosen_) { 

    if (chosen_values_[slot_id] == NULL) {
      LOG_TRACE_CAP("<add_chosen_value> NULL will be filled slot_id : %llu", slot_id);
      chosen_values_[slot_id] = new PropValue(*prop_value);
    } else {
      LOG_TRACE_CAP("<add_chosen_value> repeated occurred! slot_id: %u", slot_id);
    } 

  } else { 

    for (int i = max_chosen_ + 1; i < slot_id; i++) {
      chosen_values_.push_back(NULL);
    }
    chosen_values_.push_back(new PropValue(*prop_value));
    max_chosen_ = slot_id;
    LOG_TRACE_CAP("<add_chosen_value> push_back() slot_id : %llu", slot_id);
  }

  add_callback();

  max_chosen_mutex_.unlock();
}

/**
 * Add a new learn_value 
 */
void Captain::add_learn_value(slot_id_t slot_id, PropValue *prop_value, node_id_t node_id) {
  LOG_DEBUG("<add_learn_value> slot_id:%u from node_id:%d!", slot_id, node_id);
  add_chosen_value(slot_id, prop_value);
  
}

// all inside max_chosen_mutex_.lock() 
void Captain::add_callback() {

  LOG_TRACE_CAP("add_callback triggered");

  while (max_chosen_without_hole_ < max_chosen_) {
    
    PropValue *prop_value = chosen_values_[max_chosen_without_hole_ + 1];

    if (prop_value == NULL) {
      LOG_TRACE_CAP("prop_value == NULL (max_chosen_without_hole_ + 1):%llu)", max_chosen_without_hole_ + 1);
      break;
    }

    max_chosen_without_hole_++;

    if (prop_value->has_cmd_type()) {
      switch (prop_value->cmd_type()) {
        case SET_MASTER: {
          node_id_t old_master = view_->master_id();
          node_id_t node_id = (node_id_t)prop_value->id();
          view_->set_master(node_id);
          LOG_INFO_CAP("master_id changed from %u to %u", old_master, view_->master_id());
          if (node_id == view_->whoami()) {
            boost::mutex::scoped_lock lock(commit_ok_mutex_);
            commit_ok_ = true;
            commit_ok_cond_.notify_one();
          }
          break;
        }
      }
    } 

    if (callback_full_) {
      node_id_t node_id = node_id_t(prop_value->id()); 
      callback_full_(max_chosen_without_hole_, *prop_value, node_id);
    }
    if (callback_)
      callback_(max_chosen_without_hole_, *(prop_value->mutable_data()));
    
  } 

}

int Captain::win_size() {
  return window_size_;
}
} //  namespace ndnpaxos
