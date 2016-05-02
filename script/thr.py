import fileinput
#for line in fileinput.input(['../results/naxos/fail_SN_042919_3r_node3/N_t_10000_0_3.txt']):
records = []
thr = []
for line in open('../results/naxos/fail_SN_042919_3r_node3/N_t_10000_10_3.txt','r').readlines():
    records.append(int(line.rstrip('\n')))

print len(records)
for i in range(251, len(records) - 251):
    thr.append(records[i + 250] - records[i - 250])
    print i - 251, thr[i - 251]

thr_file = open('../results/naxos/fail_SN_042919_3r_node3/N_t_10000_10_3_final.txt', 'w')
for item in thr:
  thr_file.write("%s\n" % item)
