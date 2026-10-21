/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

LOCAL str file;
LOCAL unit *u;

str progname = "megc";
bool fwerror;
bool fdump;

struct info {
	str file;
};

/* Compilation routine. */
int
run(void *args) {
	struct info *i = args;
	file = i->file;

	if(lexinit())
		return 1;
	meminit();
	parserinit();
	checkinit();

	newunit(file);
	parse();
	check();

	lexdnit();
	memdnit();
	if(errcount())
		return 1;
	return 0;
}

int
main(int c, char **v) {
	setlocale(LC_ALL, "en-US.UTF-8");

	str file = nullptr;
	progname = v[0];

	for(int i = 1; i < c; i++) {
		str arg = v[i];

		if(*arg == '-') {
			arg++;
			switch(*arg) {
			case 'w':
				arg++;
				if(!strcmp(arg, "error"))
					fwerror = true;
				else {
					break;
				}
				continue;
			case 'd':
				fdump = true;
				continue;
			}

			erro("Unknown option %s.", v[i]);
			continue;
		}

		if(file) {
			erro("One file at once!");
			continue;
		}

		file = arg;
	}

	if(errcount())
		return 1;

	struct info args = {
		.file = file
	};

	/*
	 * TODO: Accept multiple files
	 * and run multiple threads.
	 */
	int ret = run(&args);
	return ret;
}
