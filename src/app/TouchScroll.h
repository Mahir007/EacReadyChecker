// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class QAbstractScrollArea;

// A finger drags and flicks the area's contents. A tap still reaches the widget under it, but a
// drag or a tap that stops a flick doesn't. The mouse and the touchpad scroll as before.
void enableTouchScrolling(QAbstractScrollArea *area);
