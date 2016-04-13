/**
 * Created on Mar 29, 2016
 * @author Lijing Wang OoOfreedom@gmail.com
 */

#include "client.hpp"

namespace ndnpaxos {

int main(int argc, char** argv) {

  if (argc < 4) {
    std::cerr << "Usage:Commit_Win_Size Write_Ratio(100/90/10/0) Node_Num" << std::endl;
    return 0;
  }

  ndn::Name prefix("/ndn/thu/naxos");
  int win_size = std::stoi(argv[1]);
  int ratio = std::stoi(argv[2]);
  int node_num = std::stoi(argv[3]);
  Client client(prefix, win_size, ratio, node_num);
  client.start_commit();
  return 0;
}

} // ndnpaxos

int main(int argc, char** argv) {
  return ndnpaxos::main(argc, argv);
}
