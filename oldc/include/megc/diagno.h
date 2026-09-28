/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#pragma once

#include <megc/loc.h>

int mgeterrc();

/* For debug and internal abort */
[[noreturn]]
void madeus(const char *str, ...);

void merro(const char *str, ...);

void minfo(const char *str, ...);

void mwarn(const char *str, ...);

void mnote(const char *str, ...);

void mferro(struct mloc loc, const char *str, ...);

void mfinfo(struct mloc loc, const char *str, ...);

void mfwarn(struct mloc loc, const char *str, ...);

void mfnote(struct mloc loc, const char *str, ...);
