system_ss.add(when: libbpf, if_true: files('ebpf.c', 'ebpf_rss.c'), if_false: files('ebpf_rss-stub.c'))
system_ss.add(when: ubpf, if_true: files('ubpf.c'))