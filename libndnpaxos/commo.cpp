/**
 * Created on Dec 09, 2015
 * @author Lijing Wang OoOfreedom@gmail.com
 */

#include "commo.hpp"
#include "captain.hpp"
#include <iostream>
#include <boost/thread/mutex.hpp>
#include <boost/bind.hpp>


namespace ndnpaxos {

Commo::Commo(Captain *captain, View &view, int role) 
  : captain_(captain), view_(&view), reg_ok_(false), 
    log_name_(view_->prefix()), commit_name_(view_->prefix()), log_counter_(0) {

  face_ = ndn::make_shared<ndn::Face>();
//  scheduler_ = ndn::unique_ptr<ndn::Scheduler>(new ndn::Scheduler(face_->getIoService()));
  LOG_INFO_COM("%s Init START", view_->hostname().c_str());

  for (uint32_t i = 0; i < view_->nodes_size(); i++) {
    ndn::Name new_name(view_->prefix()); 
    consumer_names_.push_back(new_name.append(view_->hostname(i)));
  }
  log_name_.append("log");
  commit_name_.append("commit");

  if (view_->if_master()) {
    ndn::Name write_name(commit_name_);
    write_name.appendNumber(0);
    // if there is only one node, then serve read and write

    LOG_INFO_COM("setInterestFilter for %s", write_name.toUri().c_str());
    face_->setInterestFilter(write_name,
                          bind(&Commo::onInterestCommit, this, _1, _2),
                          bind(&Commo::onRegisterSucceed, this, _1),
                          bind(&Commo::onRegisterFailed, this, _1, _2));
    // only register master for clients
    
//#if MODE_TYPE == 2 
    ndn::Name read_name(commit_name_);
    read_name.appendNumber(1);
    LOG_INFO_COM("setInterestFilter for %s", read_name.toUri().c_str());
    face_->setInterestFilter(read_name,
                          bind(&Commo::onInterestCommit, this, _1, _2),
                          bind(&Commo::onRegisterSucceed, this, _1),
                          bind(&Commo::onRegisterFailed, this, _1, _2));

  } else {
    if (view_->if_quorum()) {
      LOG_INFO_COM("setInterestFilter for %s", consumer_names_[view_->whoami()].toUri().c_str());
      face_->setInterestFilter(consumer_names_[view_->whoami()],
                            bind(&Commo::onInterest, this, _1, _2),
                            bind(&Commo::onRegisterSucceed, this, _1),
                            bind(&Commo::onRegisterFailed, this, _1, _2));
      LOG_INFO_COM("setInterestFilter for %s", log_name_.toUri().c_str());
      face_->setInterestFilter(log_name_,
                            bind(&Commo::onInterestLog, this, _1, _2),
                            bind(&Commo::onRegisterSucceed, this, _1),
                            bind(&Commo::onRegisterFailed, this, _1, _2));

    ndn::Name write_name(commit_name_);
    write_name.appendNumber(0);
    LOG_INFO_COM("setInterestFilter for %s", write_name.toUri().c_str());
    face_->setInterestFilter(write_name,
                          bind(&Commo::onInterestCommit, this, _1, _2),
                          bind(&Commo::onRegisterSucceed, this, _1),
                          bind(&Commo::onRegisterFailed, this, _1, _2));

    }
      LOG_INFO_COM("setInterestFilter for %s", consumer_names_[view_->whoami()].toUri().c_str());
      face_->setInterestFilter(consumer_names_[view_->whoami()],
                            bind(&Commo::onInterest, this, _1, _2),
                            bind(&Commo::onRegisterSucceed, this, _1),
                            bind(&Commo::onRegisterFailed, this, _1, _2));


    if (!view_->if_quorum() || (view_->nodes_size() == 2)) {

      // non-quorum servants need to serve reading requests
      ndn::Name read_name(commit_name_);
      read_name.appendNumber(1);
      LOG_INFO_COM("setInterestFilter for %s", read_name.toUri().c_str());
      face_->setInterestFilter(read_name,
                            bind(&Commo::onInterestCommit, this, _1, _2),
                            bind(&Commo::onRegisterSucceed, this, _1),
                            bind(&Commo::onRegisterFailed, this, _1, _2));
      }
    // none master node needs to register itself node_id and for committed value
  }

//  boost::thread listen(boost::bind(&Commo::start, this));
  if (view_->nodes_size() == 1) {
    pool_ = new pool(1);
  }
//  std::cout << "win_size_ " << captain_->win_size() << std::endl;
//  win_pool_ = new pool(captain_->win_size());
}

Commo::~Commo() {
}

void Commo::start() {
  try {
    LOG_INFO_COM("processEvents attached!");
    face_->processEvents();
  //  face_->getIoService().run();
    LOG_INFO_COM("processEvents attach Finished!");
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    face_->shutdown();
  }
} 

void Commo::handle_myself(google::protobuf::Message *msg, MsgType msg_type) {
  captain_->handle_msg(msg, msg_type);
}
void Commo::broadcast_msg(google::protobuf::Message *msg, MsgType msg_type) {
  
  if (view_->nodes_size() == 1) {
    pool_->schedule(boost::bind(&Commo::handle_myself, this, msg, msg_type));
    return;
  }

  std::string msg_str;
  msg->SerializeToString(&msg_str);
  msg_str.append(std::to_string(msg_type));
  ndn::name::Component message(reinterpret_cast<const uint8_t*>
                               (msg_str.c_str()), msg_str.size());

  for (std::set<node_id_t>::iterator it = view_->get_quorums()->begin(); it != view_->get_quorums()->end(); ++it) {
   int i = *it; 
    if (i == view_->whoami()) {
//      pool_->schedule(boost::bind(&Commo::handle_myself, this, msg, msg_type));
      LOG_DEBUG_COM("Broadcast to myself --node%d (msg_type):%s", i, msg_type_str[msg_type].c_str());
      captain_->handle_msg(msg, msg_type);
      continue;
    }

    ndn::Name new_name(consumer_names_[i]);
    new_name.append(message);

    LOG_DEBUG_COM("Broadcast to --node%d (msg_type):%s", i, msg_type_str[msg_type].c_str());
//    if (msg_type == PREPARE)
//      scheduler_->scheduleEvent(ndn::time::milliseconds(0),
//                             bind(&Commo::consume, this, new_name));
//    else // ACCEPT DECIDE 
    consume(new_name);
//    win_pool_->schedule(boost::bind(&Commo::consume, this, new_name));
  }
}

void Commo::produce(std::string &content, ndn::Name& dataName, int seconds) {
  // Create Data packet
  ndn::shared_ptr<ndn::Data> data = ndn::make_shared<ndn::Data>();
  data->setName(dataName);
  data->setFreshnessPeriod(ndn::time::seconds(seconds));
  data->setContent(reinterpret_cast<const uint8_t*>(content.c_str()), content.size());

  // Sign Data packet with default identity
  // keyChain_.sign(*data);
  keyChain_.signWithSha256(*data);

  // Return Data packet to the requester
//  std::cout << ">>Producer D: " << *data << std::endl;
  face_->put(*data);
}

void Commo::send_one_msg(google::protobuf::Message *msg, MsgType msg_type) {
//  pool_->schedule(boost::bind(&Commo::handle_myself, this, msg, msg_type));
  captain_->handle_msg(msg, msg_type);
}

void Commo::send_one_msg(google::protobuf::Message *msg, MsgType msg_type, node_id_t node_id) {

  LOG_INFO_COM("Send ONE to --%s (msg_type):%s", view_->hostname(node_id).c_str(), msg_type_str[msg_type].c_str());

  std::string msg_str;
  msg->SerializeToString(&msg_str);
  msg_str.append(std::to_string(msg_type));

  ndn::name::Component message(reinterpret_cast<const uint8_t*>
                               (msg_str.c_str()), msg_str.size());
  ndn::Name new_name(consumer_names_[node_id]);
  new_name.append(message);
 // if (msg_type == COMMIT) {
 //   //face_->getIoService().run();
 //   scheduler_->scheduleEvent(ndn::time::milliseconds(0),
 //                            bind(&Commo::consume, this, new_name));
 // } else 
//  win_pool_->schedule(boost::bind(&Commo::consume, this, new_name));
  consume(new_name);
  LOG_INFO_COM("Send ONE to --%s (msg_type):%s finished", view_->hostname(node_id).c_str(), msg_type_str[msg_type].c_str());

}

void Commo::send_one_msg(google::protobuf::Message *msg, MsgType msg_type, node_id_t node_id, ndn::Name& dataName) {

  LOG_DEBUG_COM("Reply ONE to --%s (msg_type):%s", view_->hostname(node_id).c_str(), msg_type_str[msg_type].c_str());

  std::string msg_str;
  msg->SerializeToString(&msg_str);
  msg_str.append(std::to_string(msg_type));

  produce(msg_str, dataName);
}

void Commo::consume_log_next() {
  if(view_->if_quorum() == false)
  {  
    ndn::Name name(log_name_);
    log_mut_.lock();
    log_counter_++;
    ndn::Interest interest(name.appendNumber(log_counter_));
    log_mut_.unlock();
    interest.setInterestLifetime(ndn::time::milliseconds(20000));
    interest.setMustBeFresh(true);
    face_->expressInterest(interest,
                           bind(&Commo::onDataLog, this,  _1, _2),
//                           bind(&Commo::onNack, this,  _1, _2),
                           bind(&Commo::onTimeoutLog, this, _1, 0));
    if ((log_counter_ % 3000) == 0)
      LOG_INFO("consume log %d", log_counter_);
  }
}

void Commo::consume_log(int win_size) {
  for (int i = 0; i < win_size; i++) {
    consume_log_next();  
  }
}

void Commo::produce_log(slot_id_t slot_id, PropValue *prop_value) {

  ndn::Name dataName(log_name_);
  dataName.appendNumber(slot_id).appendNumber(prop_value->id());
  LOG_TRACE_COM("produce msg-- dataName %s ", dataName.toUri().c_str());

  produce(*(prop_value->mutable_data()), dataName, 10000);
}

void Commo::onInterestLog(const ndn::InterestFilter& filter, const ndn::Interest& interest) {
  // deal with interest asking for committed_log of slot_id
  ndn::Name dataName(interest.getName());
  LOG_TRACE_COM("<< Producer onInterestLog: %s", dataName.toUri().c_str());
  slot_id_t slot_id = dataName.get(-1).toNumber();
  PropValue *prop_value = captain_->get_chosen_value(slot_id);
  if (prop_value) {
    produce(*(prop_value->mutable_data()), dataName.appendNumber(prop_value->id()), 10000);
  }
}

void Commo::onInterestCommit(const ndn::InterestFilter& filter, const ndn::Interest& interest) {
  // interest format /prefix/commit/write_or_read(0 or 1,Number)/client_id(Number)/commit_counter(Number)/value_string
  ndn::Name inName =  interest.getName();
  std::string value = inName.get(-1).toUri();
  int commit_counter = inName.get(-2).toNumber();
  int client_id = inName.get(-3).toNumber();
  int type = inName.get(-4).toNumber(); 
  if (type == 0) // if write then put it into committing process
    captain_->commit(value, inName);
  else {
    // read data and return the result directly
//    LOG_INFO("return read directly!");
    std::string data_value = "Need to change here";
    produce(data_value, inName);
  }
}

void Commo::inform_client(slot_id_t slot_id, int try_time, ndn::Name &dataName) {
  ndn::Name new_name(dataName);
  new_name.appendNumber(slot_id);
  std::string data = std::to_string(try_time);
  produce(data, new_name);
}

void Commo::onInterest(const ndn::InterestFilter& filter, const ndn::Interest& interest) {
  ndn::Name dataName(interest.getName());
  LOG_DEBUG_COM("<< Producer I: %s", dataName.toUri().c_str());
  if (view_->if_quorum() == false) {
    LOG_INFO_COM("<< Producer I: %s", dataName.toUri().c_str());
  }

  // Create new name, based on Interest's name
  ndn::name::Component request = interest.getName().get(-1);
  const uint8_t* value = request.value();
  size_t size = request.value_size();
  std::string msg_str(value, value + size);

//  pool_->schedule(boost::bind(&Commo::deal_msg, this, msg_str, dataName));
  deal_msg(msg_str, dataName);
}

void Commo::onRegisterSucceed(const ndn::InterestFilter& filter) {
  LOG_INFO_COM("onRegisterSucceed! %s", filter.getPrefix().toUri().c_str());
  // std::cerr << "onRegisterSucceed! " << filter << std::endl;
  // boost::mutex::scoped_lock lock(reg_ok_mutex_);
  // reg_ok_ = true;
  // reg_ok_cond_.notify_one();
}

void Commo::onRegisterFailed(const ndn::Name& prefix, const std::string& reason) {
  std::cerr << "ERROR: Failed to register prefix \""
            << prefix << "\" in local hub's daemon (" << reason << ")"
            << std::endl;
  face_->shutdown();
}

void Commo::deal_msg(std::string &msg_str, ndn::Name &dataName) {
//  std::string msg_str(static_cast<char*>(request.data()), request.size());
  int type = int(msg_str.back() - '0');
  google::protobuf::Message *msg = nullptr;
//    LOG_DEBUG_COM("type %d", type);
  switch(type) {
    case PREPARE: {
      msg = new MsgPrepare();
      break;
    }
    case PROMISE: {
      msg = new MsgAckPrepare();
      break;
    }
    case ACCEPT: {
      msg = new MsgAccept();
      break;
    }
    case ACCEPTED: {
      msg = new MsgAckAccept();
      break;
    }
    case DECIDE: {
      msg = new MsgDecide();
      break;
    }
    case LEARN: {
      msg = new MsgLearn();
      break;
    }                        
    case TEACH: {
      msg = new MsgTeach();
      break;
    }
    case COMMIT: {
      msg = new MsgCommit();
      break;
    }
    case COMMAND: {
      msg = new MsgCommand();
      break;
    }
  }
  msg_str.pop_back();
  msg->ParseFromString(msg_str);
  std::string text_str;
  google::protobuf::TextFormat::PrintToString(*msg, &text_str);
  LOG_TRACE_COM("deal_msg Received %s", text_str.c_str());
  captain_->handle_msg(msg, static_cast<MsgType>(type), dataName);
  LOG_TRACE_COM("deal_msg Handle finish!");
}

void Commo::deal_nack(std::string &msg_str) {
  int type = int(msg_str.back() - '0');
  if (type == COMMIT) {
    MsgCommit *msg_com = new MsgCommit();
    msg_str.pop_back();
    msg_com->ParseFromString(msg_str);
    std::string text_str;
    google::protobuf::TextFormat::PrintToString(*msg_com, &text_str);
    LOG_TRACE_COM("deal_nack Received %s", text_str.c_str());
    captain_->master_change(msg_com->mutable_prop_value());
    LOG_TRACE_COM("deal_nack Handle finish!");
  }
}

void Commo::consume(ndn::Name& name) {
  ndn::Interest interest(name);
  interest.setInterestLifetime(ndn::time::milliseconds(200));
  interest.setMustBeFresh(true);
//  std::cerr << "Sending I: " << interest << std::endl;
  face_->expressInterest(interest,
                         bind(&Commo::onData, this,  _1, _2),
                         bind(&Commo::onNack, this,  _1, _2),
                         bind(&Commo::onTimeout, this, _1, 0));
//  std::cerr << "Finish Sending I: " << interest << std::endl;
}

void Commo::onDataLog(const ndn::Interest& interest, const ndn::Data& data) {

  ndn::Name dataName(data.getName());
//  std::cout << "Consumer onDataLog interest Name" << interest.getName() << std::endl;
//  std::cout << "Consumer onDataLog data Name" << data.getName() << std::endl;
  slot_id_t slot_id = dataName.get(-2).toNumber();
  value_id_t value_id = dataName.get(-1).toNumber();
  const uint8_t* value = data.getContent().value();
  size_t size = data.getContent().value_size();
  std::string value_str(value, value + size);
  PropValue *prop_value = new PropValue(); 
  prop_value->set_data(value_str);
  prop_value->set_id(value_id);
//  pool_->schedule(boost::bind(&Commo::deal_msg, this, value_str, dataName));
  captain_->add_chosen_value(slot_id, prop_value);

  // carry on consuming
  if(view_->if_quorum() == false)
    consume_log_next();
}

void Commo::onData(const ndn::Interest& interest, const ndn::Data& data) {
  ndn::Name dataName(interest.getName());
  const uint8_t* value = data.getContent().value();
  size_t size = data.getContent().value_size();
  std::string value_str(value, value + size);
  LOG_TRACE_COM("Consumer onData get");
//  pool_->schedule(boost::bind(&Commo::deal_msg, this, value_str, dataName));
  deal_msg(value_str, dataName);
}

void Commo::onNack(const ndn::Interest& interest, const ndn::lp::Nack& nack) {
//  std::cerr << ndn::time::steady_clock::now() << " Consumer Nack " << interest.getName().toUri() << std::endl;
//  LOG_DEBUG_COM("Consumer NACK %s", interest.getName().toUri().c_str());
#if MODE_TYPE == 1
  ndn::Name name = interest.getName();
  std::string node_name = name.get(-2).toUri();
  LOG_INFO_COM("Nack!! interest %s", interest.getName().toUri().c_str());
  std::string node_id(&node_name.back());
  node_id_t old_sq = std::stoi(node_id);
  LOG_INFO_COM("old_sq %d", old_sq);
  if (old_sq >= 2) return;
  quorum_mut_.lock();

  if (view_->get_n_quorums()->empty() != true) {

  view_->print_quorums();
  view_->print_n_quorums();
  view_->remove_quorums(old_sq);
  node_id_t old_nq = *(view_->get_n_quorums()->begin());
  view_->add_quorums(old_nq);
  view_->remove_n_quorums(old_nq);
  view_->print_quorums();
  view_->print_n_quorums();

  quorum_mut_.unlock();
  MsgCommand *msg_cmd = captain_->msg_command(SET_QUORUM);
  send_one_msg(msg_cmd, COMMAND, old_nq);
  
  ndn::Name new_name(consumer_names_[old_nq]);
  new_name.append(interest.getName().get(-1));

  LOG_INFO_COM("SEND to --node%d  AGAIN", old_nq);
  consume(new_name);
  }
#endif

//  ndn::name::Component request = interest.getName().get(-1);
//  const uint8_t* value = request.value();
//  size_t size = request.value_size();
//  std::string msg_str(value, value + size);
//  
//  deal_nack(msg_str);

}

void Commo::onTimeout(const ndn::Interest& interest, int& resendTimes) {

//#if MODE_TYPE == 1
//  ndn::Name name = interest.getName();
//  std::string node_name = name.get(-2).toUri();
//  LOG_INFO_COM("Timeout!! interest %s", interest.getName().toUri().c_str());
//  std::string node_id(&node_name.back());
//  node_id_t old_sq = std::stoi(node_id);
//  LOG_INFO_COM("old_sq %d", old_sq);
//  if (old_sq >= 2) return;
//  quorum_mut_.lock();
//
//  view_->print_quorums();
//  view_->print_n_quorums();
//  view_->remove_quorums(old_sq);
//  node_id_t old_nq = *(view_->get_n_quorums()->begin());
//  view_->add_quorums(old_nq);
//  view_->remove_n_quorums(old_nq);
//  view_->print_quorums();
//  view_->print_n_quorums();
//
//  quorum_mut_.unlock();
//  MsgCommand *msg_cmd = captain_->msg_command(SET_QUORUM);
//  send_one_msg(msg_cmd, COMMAND, old_nq);
//  
//  ndn::Name new_name(consumer_names_[old_nq]);
//  new_name.append(interest.getName().get(-1));
//
//  LOG_INFO_COM("SEND to --node%d  AGAIN", old_nq);
//  consume(new_name);
//#endif

//  LOG_DEBUG_COM("Consumer Timeout %s, count %d", interest.getName().toUri().c_str(), resendTimes);
//  std::cerr << ndn::time::steady_clock::now() << " Consumer Timeout " << interest.getName().toUri() << " count " << resendTimes << std::endl;
  
//  if (resendTimes < MAX_TIMEOUT) {
////    std::cerr << "Rexpress interest " << interest << std::endl;
//    ndn::Interest interest_new(interest);
//    interest_new.refreshNonce();
////    std::cerr << "Rexpress interest_new " << interest_new << std::endl;
//    face_->expressInterest(interest_new,
//                           bind(&Commo::onData, this,  _1, _2),
//                           bind(&Commo::onNack, this,  _1, _2),
//                           bind(&Commo::onTimeout, this, _1, resendTimes + 1));
//  } else {
//    ndn::name::Component request = interest.getName().get(-1);
//    const uint8_t* value = request.value();
//    size_t size = request.value_size();
//    std::string msg_str(value, value + size);
//    
//    deal_nack(msg_str);
//  }

}

ndn::shared_ptr<ndn::Face> Commo::getFace() {
  return face_;
}

void Commo::stop() {
  face_->shutdown();
//  face_->getIoService().stop();
}

} // namespace ndnpaxos
