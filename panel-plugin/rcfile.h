/* wmdock xfce4 plugin by Andre Ellguth
 * rcfile.h
 *
 * Authors:
 *   Andre Ellguth <andre@ellguth.com>
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

#ifndef __RCFILE_H__
#define __RCFILE_H__

#define RCKEY_CMDLIST              (const gchar *) "cmds"
#define RCKEY_CMDCNT               (const gchar *) "cmdcnt"
#define RCKEY_DISPTILE             (const gchar *) "disptile"
#define RCKEY_DISPPROPBTN          (const gchar *) "disppropbtn"
#define RCKEY_DISPADDONLYWM        (const gchar *) "dispaddonlywm"
#define RCKEY_PANELOFF             (const gchar *) "paneloff"
#define RCKEY_PANELOFFIGNOREOFFSET (const gchar *) "paneloffignoreoffset"
#define RCKEY_PANELOFFKEEPABOVE    (const gchar *) "paneloffkeepabove"
#define RCKEY_DAFILTER             (const gchar *) "dafilter"
#define RCKEY_ANCHORPOS            (const gchar *) "anchorpos"
#define RCKEY_GLUELIST             (const gchar *) "glues"

#define RC_LIST_DELIMITER   (const gchar *) ";"
#define RC_GLUE_DELIMITER   (const gchar *) ","

/* Prototypes */
void wmdock_read_rc_file (XfcePanelPlugin *);
void wmdock_write_rc_file (XfcePanelPlugin *);

#endif /* __RCFILE_H__ */
