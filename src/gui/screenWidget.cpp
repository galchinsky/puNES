/*
 *  Copyright (C) 2010-2019 Fabio Cavallo (aka FHorse)
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <QtCore/QtGlobal>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QtCore/QMimeData>
#endif
#include <QtCore/QUrl>
#include "screenWidget.moc"
#include "settingsObject.hpp"
#include "conf.h"
#include "tas.h"
#include "timeline.h"
#include "gui.h"
#include "patcher.h"
#include "recent_roms.h"
#if defined (WITH_OPENGL)
#include "opengl.h"
#endif

screenWidget::screenWidget(QWidget *parent, mainWindow *mw) : QWidget(parent) {
#if defined (__WIN32__)
#if defined (WITH_OPENGL)
	memset(&data, 0x00, sizeof(data));
	data.qt = (WNDPROC)GetWindowLongPtr((HWND) winId(), GWLP_WNDPROC);

	// applico un sfondo nero
	parent->setStyleSheet("background-color: black");
#endif
	target = nullptr;
#endif

	mwin = mw;

	// se non faccio questa chiamata, la versione SDL crasha all'avvio
	winId();

	setUpdatesEnabled(false);

	setAcceptDrops(true);

	setFocusPolicy(Qt::StrongFocus);
	setFocus(Qt::ActiveWindowFocusReason);

	setMouseTracking(true);

	installEventFilter(this);
}
screenWidget::~screenWidget() {}
#if defined (__WIN32__)
#if defined (WITH_OPENGL)
void screenWidget::controlEventFilter() {
	data.tmp = (WNDPROC)GetWindowLongPtr((HWND) winId(), GWLP_WNDPROC);

	if ((data.tmp != data.sdl) && (data.tmp != data.qt)) {
		data.sdl = data.tmp;
	}

	if (data.tmp != data.qt) {
		SetWindowLongPtr((HWND) winId(), GWLP_WNDPROC, (LONG_PTR) data.qt);
	}
}
#endif
void screenWidget::cursor_init() {
	target = new QCursor(QPixmap(":/pointers/pointers/target_32x32.xpm"), -1, -1);
}
void screenWidget::cursor_set() {
	if (input_draw_target() == TRUE) {
		setCursor((*target));
	} else {
		unsetCursor();
	}
}
void screenWidget::cursor_hide(BYTE hide) {
	if (hide == TRUE) {
		setCursor(Qt::BlankCursor);
	} else {
		cursor_set();
	}
}
#endif

void screenWidget::dragEnterEvent(QDragEnterEvent *e) {
	if (e->mimeData()->hasUrls()) {
		e->acceptProposedAction();
	}
}
void screenWidget::dropEvent(QDropEvent *e) {
	foreach (const QUrl &url, e->mimeData()->urls()){
		QFileInfo fileinfo(url.toLocalFile());
		_uncompress_archive *archive;
		BYTE is_rom = FALSE, is_patch = FALSE, rc;
		uTCHAR *rom, *patch = NULL;

		if ((cfg->cheat_mode == GAMEGENIE_MODE) && (gamegenie.phase == GG_EXECUTE)) {
			rom = gamegenie.rom;
		} else {
			rom = info.rom.file;
		}

		archive = uncompress_archive_alloc(uQStringCD(fileinfo.absoluteFilePath()), &rc);

		if (rc == UNCOMPRESS_EXIT_OK) {
			if (archive->rom.count > 0) {
				is_rom = TRUE;
			}
			if (archive->patch.count > 0) {
				is_patch = TRUE;
			}
			if ((is_patch == TRUE) && (is_rom == FALSE) && !info.rom.file[0]) {
				is_patch = FALSE;
			}
			if (is_rom) {
				switch ((rc = uncompress_archive_extract_file(archive,UNCOMPRESS_TYPE_ROM))) {
					case UNCOMPRESS_EXIT_OK:
						rom = uncompress_archive_extracted_file_name(archive, UNCOMPRESS_TYPE_ROM);
						break;
					case UNCOMPRESS_EXIT_ERROR_ON_UNCOMP:
						return;
					default:
						break;
				}
			}
			if (is_patch) {
				switch ((rc = uncompress_archive_extract_file(archive,UNCOMPRESS_TYPE_PATCH))) {
					case UNCOMPRESS_EXIT_OK:
						patch = uncompress_archive_extracted_file_name(archive, UNCOMPRESS_TYPE_PATCH);
						break;
					case UNCOMPRESS_EXIT_ERROR_ON_UNCOMP:
						return;
					default:
						is_patch = FALSE;
						break;
				}
			}
			uncompress_archive_free(archive);
		} else if (rc == UNCOMPRESS_EXIT_IS_NOT_COMP) {
			 if ((fileinfo.suffix().toLower() == "ips") && info.rom.file[0]) {
				is_patch = TRUE;
				patch = uQStringCD(fileinfo.absoluteFilePath());
			} else {
				is_rom = TRUE;
				rom = uQStringCD(fileinfo.absoluteFilePath());
			}
		}

		if (is_patch) {
			patcher.file = emu_ustrncpy(patcher.file, patch);
		}

		mwin->change_rom(rom);
		activateWindow();
		gui_set_focus();
		break;
	}
}
bool screenWidget::eventFilter(QObject *obj, QEvent *event) {
	static QMouseEvent *mouseEvent;
	static QKeyEvent *keyEvent;
	static DBWORD keyval;

	if (event->type() == QEvent::KeyPress) {
		keyEvent = ((QKeyEvent *)event);
		keyval = inpObject::kbd_keyval_decode(keyEvent);

		if (keyval == gui.key.tl) {
			if (!tl.key) {
				mwin->statusbar->timeline->timeline_pressed(&tl.key);
			}
			return (true);
#if !defined (RELEASE)
		} else if (keyval == Qt::Key_Insert) {
			info.snd_info = !info.snd_info;
#endif
		} else if (keyval == Qt::Key_Left) {
			if (tl.key) {
				int snap = mwin->statusbar->timeline->value();

				mwin->statusbar->timeline->setValue(snap - 1, true);
				return (true);
			}
		} else if (keyval == Qt::Key_Right) {
			if (tl.key) {
				int snap = mwin->statusbar->timeline->value();

				mwin->statusbar->timeline->setValue(snap + 1, true);
				return (true);
			}
		}

		if (!tas.type) {
			for (BYTE i = PORT1; i < PORT_MAX; i++) {
				if (port_funct[i].input_decode_event && (port_funct[i].input_decode_event(PRESSED,
						keyEvent->isAutoRepeat(), keyval, KEYBOARD, &port[i]) == EXIT_OK)) {
					return (true);
				}
			}
		}
	} else if (event->type() == QEvent::KeyRelease) {
		keyEvent = ((QKeyEvent *)event);
		keyval = inpObject::kbd_keyval_decode(keyEvent);

		if (keyval == gui.key.tl) {
			if (tl.key) {
				mwin->statusbar->timeline->timeline_released(&tl.key);
			}
			return (true);
		}

		if (!tas.type) {
			for (BYTE i = PORT1; i < PORT_MAX; i++) {
				if (port_funct[i].input_decode_event && (port_funct[i].input_decode_event(RELEASED,
						keyEvent->isAutoRepeat(), keyval, KEYBOARD, &port[i]) == EXIT_OK)) {
					return (true);
				}
			}
		}
	} else if (event->type() == QEvent::MouseButtonPress) {
		mouseEvent = ((QMouseEvent *)event);

		if (mouseEvent->button() == Qt::LeftButton) {
			gmouse.left = TRUE;
		} else if (mouseEvent->button() == Qt::RightButton) {
			gmouse.right = TRUE;
		}
	} else if (event->type() == QEvent::MouseButtonDblClick) {
		mouseEvent = ((QMouseEvent *)event);

		if (mouseEvent->button() == Qt::LeftButton) {
			gmouse.left = TRUE;
		} else if (mouseEvent->button() == Qt::RightButton) {
			gmouse.right = TRUE;
		}
	} else if (event->type() == QEvent::MouseButtonRelease) {
		mouseEvent = ((QMouseEvent *)event);

		if (mouseEvent->button() == Qt::LeftButton) {
			gmouse.left = FALSE;
		} else if (mouseEvent->button() == Qt::RightButton) {
			gmouse.right = FALSE;
		}
	} else if (event->type() == QEvent::MouseMove) {
		mouseEvent = ((QMouseEvent *)event);

		gmouse.x = mouseEvent->x();
		gmouse.y = mouseEvent->y();
	}

	return (QObject::eventFilter(obj, event));
}
