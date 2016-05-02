import fileinput
#for line in fileinput.input(['../results/naxos/fail_SN_042919_3r_node3/N_t_10000_0_3.txt']):
records = range(1, 5000)
records = []
thr = []
#for i in range(1, 5000)
#for line in open('../results/naxos/fail_SN_042919_3r_node3/N_t_10000_10_3.txt','r').readlines():
#    records.append(int(line.rstrip('\n')))

for i in range(5001, 6000):
    records.append(5000)
records.append(5500)
for i in range(6002, 10000):
    records.append(i - 1000)

for i in range(251, len(records) - 251):
    thr.append(records[i + 250] - records[i - 250])
    print i - 251, thr[i - 251]

thr_file = open('../results/naxos/fail_NonP/non_r.txt', 'w')
for item in thr:
  thr_file.write("%s\n" % item)
