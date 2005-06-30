/*
 *  csync2 - cluster synchronisation tool, 2nd generation
 *  LINBIT Information Technologies GmbH <http://www.linbit.com>
 *  Copyright (C) 2004, 2005  Clifford Wolf <clifford@clifford.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sqlite.h>

static sqlite *db = 0;

extern const char *url_encode(const char * in);

void sql(const char *fmt, ...)
{
	char *sql;
	va_list ap;
	int rc, busyc = 0;

	va_start(ap, fmt);
	vasprintf(&sql, fmt, ap);
	va_end(ap);

	while (1) {
		rc = sqlite_exec(db, sql, 0, 0, 0);
		if ( rc != SQLITE_BUSY ) break;
		if (busyc++ > 60) {
			fprintf(stderr, "** Database busy for long time [%d secs]: %s", busyc, sql);
		}
		sleep(1);
	}

	if ( rc != SQLITE_OK ) {
		fprintf(stderr, "** Database Error %d in '%s'!\n", rc, sql);
		sqlite_exec(db, "COMMIT TRANSACTION", 0, 0, 0);
		exit(1);
	}

	free(sql);
}

int main(int argc, char **argv)
{
	FILE *fseh;
	char line[4096], *c;
	int in_transaction = 0;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s dbfile directory\n", argv[0]);
		exit(1);
	}

	db = sqlite_open(argv[1], 0, 0);
	if (!db) {
		fprintf(stderr, "** Can't open database file: %s\n", argv[1]);
		exit(1);
	}

	{
		char command[strlen(argv[2]) + 100];
		sprintf(command, "./cs2hintd_fseh.exe '%s'", argv[2]);
		fseh = popen(command, "r");
		if (!fseh) {
			fprintf(stderr, "** Can't start cs2hintd_fseh.exe!\n");
			exit(1);
		}
	}

	while (fgets(line, 4096, fseh) != NULL)
	{
		line[4095] = 0;
		if ((c=strchr(line, '\n')) != NULL) *c = 0;
		if ((c=strchr(line, '\r')) != NULL) *c = 0;
		if (strlen(line) < 3) continue;

		if (*line == '-' && in_transaction) {
			sql("COMMIT TRANSACTION");
			fprintf(stderr, "** Changes to hint database committed.\n");
			in_transaction = 0;
		}

		if (*line == '+') {
			if (!in_transaction) {
				sql("BEGIN TRANSACTION");
				in_transaction = 1;
			}

			sql("INSERT INTO hint ( filename, recursive ) VALUES ( '%s', 0 )", url_encode(line+2));
			fprintf(stderr, "** Added to DB: %s\n", line+2);
		}
	}

	fprintf(stderr, "** Looks like cs2hintd_fseh.exe terminated!\n");
	exit(1);
}
