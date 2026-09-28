/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

LOCAL str file;

str progname = "megc";
bool fwerror;
bool fdump;

struct info {
	str file;
};

/* Compilation routine. */
int run(void *args) {
	struct info *i = args;
	file = i->file;

	meminit();
	if (lexinit())
		return 1;

	lexdnit();
	memdnit();
	return 0;
}

int main(int c, char **v) {
	setlocale(LC_ALL, "");

	str file = nullptr;
	progname = v[0];

	for (int i = 1; i < c; i++) {
		str arg = v[i];

		if (*arg == '-') {
			arg++;
			switch (*arg) {
			case 'w':
				arg++;
				if (strcmp(arg, "error"))
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

		if (file) {
			erro("One file at once!");
			continue;
		}

		file = arg;
	}

	struct info args = {
		.file = file
	};

	thrd_t s;
	int ret = thrd_create(&s, run, &args);
	if (ret != thrd_success) {
		adeus("Can't create the compiling thread.");
	}

	ret = 0;
	thrd_join(s, &ret);
	return ret;
}
