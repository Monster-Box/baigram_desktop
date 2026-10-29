/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/dev_tools_box.h"

#include "base/algorithm.h"
#include "base/invoke_queued.h"
#include "boxes/abstract_box.h"
#include "lang/lang_keys.h"
#include "logs.h"
#include "ui/layers/box_content.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"

#include <QtCore/QMutex>
#include <QtWidgets/QApplication>

namespace {
constexpr auto kMaxLogLines = 200;
constexpr auto kMaxLogChars = 4096;

[[nodiscard]] QString LimitedLog(const QString &log) {
	auto lines = log.split('\n');
	if (lines.size() > kMaxLogLines) {
		lines = lines.mid(lines.size() - kMaxLogLines);
	}
	auto result = lines.join('\n');
	if (result.size() > kMaxLogChars) {
		// A huge single string makes the text engine choke on layout.
		result = result.mid(result.size() - kMaxLogChars);
	}
	return result;
}

struct LogSink {
	QMutex mutex;
	QString pending;
	bool scheduled = false;
};

} // namespace

void DevToolsBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(u"Dev Tools"_q);

	const auto layout = box->verticalLayout();

	const auto logText = box->lifetime().make_state<rpl::variable<QString>>(
		QString());
	layout->add(
		object_ptr<Ui::FlatLabel>(
			layout,
			logText->value(),
			st::aboutLabel),
		st::boxRowPadding);

	const auto sink = box->lifetime().make_state<LogSink>();

	// Logs come from any thread and may come in bursts, so never touch widgets
	// right in the stream handler - collect them and update the label once per
	// main thread event loop iteration.
	Logs::logsStream(
	) | rpl::on_next([=](const QString &entry) {
		const auto schedule = [&] {
			QMutexLocker lock(&sink->mutex);
			sink->pending += entry;
			if (sink->scheduled) {
				return false;
			}
			sink->scheduled = true;
			return true;
		}();
		if (!schedule) {
			return;
		}
		InvokeQueued(box, [=] {
			auto entry = QString();
			{
				QMutexLocker lock(&sink->mutex);
				entry = base::take(sink->pending);
				sink->scheduled = false;
			}
			if (!entry.isEmpty()) {
				*logText = LimitedLog(logText->current() + entry);
			}
		});
	}, box->lifetime());

	const auto dump = layout->add(
		object_ptr<Ui::LinkButton>(
			layout,
			u"Dump widget tree"_q),
		st::boxRowPadding);
	dump->setClickedCallback([=] {
		auto tree = QString();
		for (const auto widget : QApplication::allWidgets()) {
			tree += QString::fromLatin1(widget->metaObject()->className());
			if (!widget->objectName().isEmpty()) {
				tree += u" \""_q + widget->objectName() + u"\""_q;
			}
			tree += '\n';
		}
		Logs::writeDebug(u"[devtools] widget tree:\n"_q + tree);
	});

	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	box->setWidth(st::aboutWidth);
}

namespace Core {

void ShowDevTools() {
	Ui::show(Box(DevToolsBox));
}

} // namespace Core
