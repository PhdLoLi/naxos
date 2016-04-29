/**
 * Created on Dec 09, 2015
 * @author Lijing Wang OoOfreedom@gmail.com
 */
#pragma once

#include "internal_types.hpp"
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/thread/mutex.hpp>

#include <fstream>
#include <unistd.h>
#include <string>
#include <stdlib.h>
#include <chrono>

namespace ndnpaxos {

class Client {
 public:
  Client(ndn::Name prefix, int commit_win, int ratio, int read_node);
  ~Client();
  void attach();
  void stop();
  void start_commit();
  void consume(ndn::Name &);

 private:
  void onRegisterSucceed(const ndn::InterestFilter& filter) {
    LOG_INFO_COM("onRegisterSucceed! %s", filter.getPrefix().toUri().c_str());
  }

  void onRegisterFailed(const ndn::Name& prefix, const std::string& reason) {
    std::cerr << "ERROR: Failed to register prefix \""
              << prefix << "\" in local hub's daemon (" << reason << ")"
              << std::endl;
    face_->shutdown();
  }

  void onInterest(const ndn::InterestFilter& filter, const ndn::Interest& interest) {
    // deal with interest asking for committed_log of slot_id
   }

  void onTimeout(const ndn::Interest& interest);
  void onData(const ndn::Interest& interest, const ndn::Data& data);  

  ndn::shared_ptr<ndn::Face> face_;
  ndn::Name prefix_;
  int com_win_;
  int ratio_;
  int read_node_;
  int write_or_read_;
  

  slot_id_t commit_counter_;
  slot_id_t thr_counter_;
  slot_id_t rand_counter_;

  boost::mutex counter_mut_;
  boost::mutex thr_mut_;
  
  std::vector<uint64_t> periods_;
  std::vector<uint64_t> throughputs_;
  std::vector<int> trytimes_;
  std::vector<std::chrono::high_resolution_clock::time_point> starts_;
  
  bool recording_;
  bool done_;

};

}  //  namespace ndnpaxos
