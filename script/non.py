import fileinput
#for line in fileinput.input(['../results/naxos/fail_SN_042919_3r_node3/N_t_10000_0_3.txt']):
records = []
thr = []
thr_50 = []
thr_100 = []
for i in range(1, 5001):
    records.append(i) 
    
#for line in open('../results/naxos/fail_SN_042919_3r_node3/N_t_10000_10_3.txt','r').readlines():
#    records.append(int(line.rstrip('\n')))

for i in range(5001, 5200):
    records.append(5000)
for i in range(1, 66):
    records.append(5000 + i * 4)
for i in range(5264, 10001):
    records.append(i)

for i in range(0, len(records) - 100):
    thr_100.append((records[i + 100] - records[i]) * 5)

for i in range(0, len(records) - 50):
    thr_50.append((records[i + 50] - records[i]) * 10)

for i in range(0, len(records) - 500):
    thr.append((records[i + 500] - records[i]))

thr_file = open('../results/naxos/fail_NonP/link_thr.txt', 'w')
for item in thr:
  thr_file.write("%s\n" % item)

thr_file_50 = open('../results/naxos/fail_NonP/link_thr_50.txt', 'w')
for item in thr_50:
  thr_file_50.write("%s\n" % item)

thr_file_100 = open('../results/naxos/fail_NonP/link_thr_100.txt', 'w')
for item in thr_100:
  thr_file_100.write("%s\n" % item)

rec_file = open('../results/naxos/fail_NonP/link_rec.txt', 'w')
for item in records:
  rec_file.write("%s\n" % item)
