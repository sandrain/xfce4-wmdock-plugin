/* wmdock xfce4 plugin by Andre Ellguth
 * Debug output.
 *
 * $Id: debug.c 10 2012-05-29 06:29:21Z ellguth $
 *
 * Authors:
 *   Andre Ellguth <ellguth@ibh.de>
 *
 * License:
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this package; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "debug.h"
#include "wmdock.h"

#if DEBUG
static FILE *fp = (FILE *) NULL;
#endif

void init_debug()
{
#if DEBUG
	char debugFile[BUF_MAX];

	sprintf(debugFile, "%s/wmdock-debug.%d", g_get_tmp_dir(), getpid());
	fp = fopen(debugFile, "w");
	if(!fp) fp = stderr;

	debug("debug.c: Debug initialized.");
#endif
}


void debug(const char *format, ...)
{
#if DEBUG
	char buf[BUF_MAX];
	va_list args;
	time_t curtime;
	struct tm *loctime;

	if(!fp) {
		return;
	}

	va_start(args, format);
    curtime = time (NULL);
    loctime = localtime (&curtime);
    strftime(buf, BUF_MAX, "%B %d %I:%M:%S", loctime);

	fprintf(fp, "%s: ", buf);
	vfprintf(fp, format, args);
	fprintf(fp, "\n");
	fflush(fp);

	va_end(args);
#endif /* DEBUG */
}


