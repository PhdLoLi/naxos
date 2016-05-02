/**
 * Created on: Dec 08, 2015
 * @Author: Lijing OoOfreedom@gmail.com
 */

#include "view.hpp"
#include <boost/filesystem.hpp>
#include <math.h>
#include <stdlib.h>

namespace ndnpaxos {

View::View(node_id_t node_id, std::string cf) 
  : node_id_(node_id), master_id_(0), period_(500), length_(100000), db_name_("Lijing.db") {

  LOG_INFO("Restart NFD and Sleep for 2 seconds");
  std::string nfd = "nfd-stop; nfd-start";
  system(nfd.c_str());
  sleep(3);

  LOG_INFO("loading config file %s ...", cf.c_str());
	
	YAML::Node config;

  if (cf.empty()) {
    // default only one node
	  config = YAML::LoadFile("config/localhost-1.yaml");
    node_id_ = 0;
  } else {
	  config = YAML::LoadFile(cf);
  }
  if (config == NULL) {
    printf("cannot open config file: %s.", cf.c_str());
  }

  prefix_ = config["prefix"].as<std::string>();
	YAML::Node nodes = config["host"];
  YAML::Node lease = config["lease"];
  YAML::Node db = config["db"];

  size_ =nodes.size();
  q_size_ = ceil((size_ + 1) / 2.0);

  LOG_INFO("size: %d    q_size: %d", size_, q_size_);
  for (std::size_t i = 0; i < nodes.size(); i++) {

		YAML::Node node = nodes[i];

		std::string name = node["name"].as<std::string>();
		std::string addr = node["addr"].as<std::string>();
    // set a node in view
    std::string local_host("localhost");
    if (addr.compare(local_host) != 0) {
#if MODE_TYPE == 1
      if ((node_id_ < q_size_) && (i > node_id_)) {
        std::string cmd_node = "nfdc register " + prefix_ + "/" + name + " tcp://" + addr;
        system(cmd_node.c_str());
        LOG_INFO("After Running %s", cmd_node.c_str());
      }
#else
      if ((node_id_ == 0) && (i > 0 && i < q_size_)) {
        std::string cmd_node = "nfdc register " + prefix_ + "/" + name + " tcp://" + addr;
        system(cmd_node.c_str());
        LOG_INFO("After Running %s", cmd_node.c_str());
      }
#endif
      if ((node_id_ >= q_size_) && (i == size_ - node_id_)) {
        std::string cmd_log = "nfdc register " + prefix_ + "/log" + " tcp://" + addr;
        system(cmd_log.c_str());
        LOG_INFO("After Running %s", cmd_log.c_str());
      }
 
    } else {
      LOG_INFO("no need to nfdc register %s/%s tcp://localhost", 
               prefix_.c_str(), name.c_str());
    }

    host_info_t host_info = host_info_t(name, addr);
    host_nodes_.push_back(host_info);
    if (i < q_size_) {
      quorums_.emplace(i);
    } else {
      n_quorums_.emplace(i);
    }
    print_quorums();
    print_n_quorums();
  }

  if (lease) {
    master_id_ = lease["master_id"].as<int>();
    period_ = lease["period"].as<int>();
    length_ = lease["length"].as<int>();
  } else {
    LOG_INFO("No lease Node Found, using default master_id/0 period/500 length/100000");
  }
  
  if (db) {
    std::string path = db["path"].as<std::string>();
    std::string db_name = db["name"].as<std::string>();
	  boost::filesystem::path dir(path);
    dir.append(host_nodes_[node_id_].name, boost::filesystem::path::codecvt());
    if(!boost::filesystem::exists(dir)) {
	    if(boost::filesystem::create_directories(dir)) {
//	    	std::cout << "Success" << "\n";
	    }
    }
    dir.append(db_name, boost::filesystem::path::codecvt());
    db_name_ = dir.string();
  }


  if (node_id_ >= size_) {
    std::cout << "Node_Id " << node_id_ << " > host_nodes_.size " << size_ << "Invalid!" << std::endl;
    std::cout << "Set Node_Id = 0" << std::endl;
    node_id_ = 0;
  }
  LOG_INFO("config file loaded");
 
}

std::set<node_id_t> * View::get_quorums() {
  return &quorums_;
}

std::set<node_id_t> * View::get_n_quorums() {
  return &n_quorums_;
}

std::vector<host_info_t> * View::get_host_nodes() {
  return &host_nodes_;
}

node_id_t View::whoami() {
  return node_id_;
}

bool View::if_master() {
  return node_id_ == master_id_ ? true : false; 
}

bool View::if_quorum() {
  return quorums_.find(node_id_) != quorums_.end() ? true : false;
//  return node_id_ < q_size_ ? true : false; 
}

void View::set_master(node_id_t node_id) {
  master_id_ = node_id;
}

std::string View::hostname() {
  return host_nodes_[node_id_].name;
}

std::string View::address() {
  return host_nodes_[node_id_].addr;
}

std::string View::prefix() {
  return prefix_;
}

std::string View::hostname(node_id_t node_id) {
  return host_nodes_[node_id].name;
}

std::string View::address(node_id_t node_id) {
  return host_nodes_[node_id].addr;
}

node_id_t View::master_id() {
  return master_id_;
}

std::string View::db_name() {
  return db_name_;
}

uint64_t View::nodes_size() {
  return size_;
}

node_id_t View::quorum_size() {
  return q_size_;
}

uint32_t View::period() {
  return period_;
}

uint32_t View::length() {
  return length_;
}


void View::print_host_nodes() {
  std::cout << "-----*-*-*-*-*-*-*-*-*------" << std::endl;
  std::cout << "\t My Node" << std::endl;
  std::cout << "\tNode_ID: " << node_id_ << std::endl;
  std::cout << "\tName: " << host_nodes_[node_id_].name << std::endl;
  std::cout << "\tAddr: " << host_nodes_[node_id_].addr << std::endl;
  std::cout << "-----*-*-*-*-*-*-*-*-*------\n" << std::endl;
  std::cout << "\t Nodes INFO" << std::endl;
  for (int i = 0; i < size_; i++) {
    std::cout << "-*-*-*-*-*-*-*-*-*-*-*-*-*-*" << std::endl;
    std::cout << "\tNode_ID: " << i << std::endl;
    std::cout << "\tName: " << host_nodes_[i].name << std::endl;
    std::cout << "\tAddr: " << host_nodes_[i].addr << std::endl;
    std::cout << "-*-*-*-*-*-*-*-*-*-*-*-*-*-*" << std::endl;
  }
  std::cout << "\tTotal Size: " << size_ << std::endl;
  std::cout << "-----*-*-*-*-*-*-*-*-*------\n" << std::endl;
  std::cout << "\t Lease INFO" << std::endl;
  std::cout << "  -----*-*-*-*-*-*-*-----   " << std::endl;
  std::cout << "\t Master_ID: " << master_id_ << std::endl;
  std::cout << "\t Period: " << period_ << std::endl;
  std::cout << "\t Length: " << length_ << std::endl;
  std::cout << "-----*-*-*-*-*-*-*-*-*------\n" << std::endl;
  std::cout << "\t DB INFO" << std::endl;
  std::cout << "  -----*-*-*-*-*-*-*-----   " << std::endl;
  std::cout << "\t DB_Name: " << db_name_ << std::endl;
  std::cout << "-*-*-*-*-*-*-*-*-*-*-*-*-*-*" << std::endl;
}

} // namespace ndnpaxos 
