import fileinput
#for line in fileinput.input(['../results/naxos/fail_SN_042919_3r_node3/N_t_10000_0_3.txt']):
records = []
thr = []
thr_50 = []
thr_100 = []
for line in open('../results/naxos/fail_SQ_050213_3r_node3/N_t_10000_100_3.txt','r').readlines():
    records.append(int(line.rstrip('\n')))

print len(records)
#for i in range(251, len(records) - 251):
#    thr.append(records[i + 250] - records[i - 250])
#    print i - 251, thr[i - 251]


for i in range(0, len(records) - 100):
    thr_100.append((records[i + 100] - records[i]) * 5)

for i in range(0, len(records) - 50):
    thr_50.append((records[i + 50] - records[i]) * 10)

for i in range(0, len(records) - 500):
    thr.append((records[i + 500] - records[i]))


thr_file = open('../results/naxos/final/SQ_thr.txt', 'w')
for item in thr:
  thr_file.write("%s\n" % item)

thr_file_50 = open('../results/naxos/final/SQ_thr_50.txt', 'w')
for item in thr_50:
  thr_file_50.write("%s\n" % item)

thr_file_100 = open('../results/naxos/final/SQ_thr_100.txt', 'w')
for item in thr_100:
  thr_file_100.write("%s\n" % item)


