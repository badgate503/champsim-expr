#!/usr/bin/env python3.11
import os

def parse_tracelist(path='./tracelist'):
	traces = set()
	with open(path) as f:
		for line in f:
			line = line.strip()
			if not line or ':' not in line:
				continue
			_, rest = line.split(':', 1)
			for t in rest.strip().split():
				traces.add(t)
	return traces

def scan_log_dir(log_dir='log'):
	txt = set()
	log = set()
	for f in os.listdir(log_dir):
		if f.endswith('.txt'):
			txt.add(f[:-4])
		elif f.endswith('.log'):
			log.add(f[:-4])
	return txt, log

if __name__ == '__main__':
	traces = parse_tracelist()
	txt, logf = scan_log_dir()
	not_loaded = sorted([t for t in traces if t not in txt and t not in logf])
	loaded_not_done = sorted([t for t in traces if t in logf and t not in txt])
	print('未加载的 trace:', not_loaded)
	print('已加载但未跑完的 trace:', '\n'.join(loaded_not_done), len(loaded_not_done))
