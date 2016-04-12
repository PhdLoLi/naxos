/**
 * Created on: Dec 08, 2015
 * @Author: Lijing OoOfreedom@gmail.com 
 */

#pragma once
#include "internal_types.hpp"
#include <set>
#include <vector>
#include <yaml-cpp/yaml.h>
#include <iostream>
namespace ndnpaxos {
class View {
 public:
//  View(set<node_id_t> &nodes);
  View(node_id_t node_id, std::string config_file);
  std::set<node_id_t> *get_quorums();
  std::set<node_id_t> *get_n_quorums();
  std::vector<host_info_t> *get_host_nodes();
  node_id_t whoami();
  bool if_master();
  bool if_quorum();
  void set_master(node_id_t);

  std::string hostname();
  std::string address();
  std::string prefix();
  std::string hostname(node_id_t);
  std::string address(node_id_t);
  node_id_t master_id();
  std::string db_name();

  uint64_t nodes_size();
  node_id_t quorum_size();
  uint32_t period();
  uint32_t length();


  void print_host_nodes();

 private:
  std::string prefix_;
  node_id_t node_id_;
  // only node_id
  std::set<node_id_t> quorums_;
  std::set<node_id_t> n_quorums_;
  // full host_info
  std::vector<host_info_t> host_nodes_;
  uint64_t size_;
  node_id_t master_id_;
  uint32_t period_;
  // total number of lease length_;
  uint32_t length_;
  node_id_t q_size_;

  std::string db_name_;
};
} // namespace ndnpaxos
