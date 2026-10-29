/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/bytes.h"
#include "base/weak_ptr.h"
#include "rpl/variable.h"

#include <QtCore/QByteArray>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <optional>

namespace Main {
class Account;
} // namespace Main

namespace MTP {
class Config;
class DcOptions;
} // namespace MTP

namespace Core {

struct ServerConfig {
	QString id;
	QString ip;
	QString name;
	int port = 0;
	QString pubkey; // PEM public key, or "NULL"/empty if absent.
	QString systrayLight;
	QString systrayDark;
	QString deeplink;
	QString dflink;
	QString website;
	QString changelogLink;
	QString aboutText;

	[[nodiscard]] bool validDc() const {
		return !id.isEmpty() && !ip.isEmpty() && (port > 0);
	}
	[[nodiscard]] bool hasPubKey() const {
		return !pubkey.isEmpty() && pubkey != u"NULL"_q;
	}
	[[nodiscard]] QString effectiveDeeplink() const {
		return deeplink.isEmpty() ? u"tg"_q : deeplink;
	}
	[[nodiscard]] QString effectiveDflink() const {
		return dflink;
	}
};

class ServerConfigManager final : public QObject {
public:
	ServerConfigManager();

	static ServerConfigManager &Instance();

	void request(
		const QString &id,
		Fn<void(const ServerConfig&)> onDone,
		Fn<void(const QString&)> onFail);
	void cancel();
	void resetToDefault();
	void setCurrent(const ServerConfig &config);

	[[nodiscard]] const rpl::variable<ServerConfig> &current() const {
		return _current;
	}

private:
	QNetworkAccessManager _manager;
	QPointer<QNetworkReply> _reply;
	rpl::variable<ServerConfig> _current;
};

void SaveServerBinding(
	not_null<Main::Account*> account,
	const ServerConfig &config);
[[nodiscard]] std::optional<ServerConfig> ReadServerBinding(
	not_null<Main::Account*> account);
void ApplyServerBindingToConfig(
	MTP::Config &config,
	const ServerConfig &server);
void ApplyServerBindingToAccount(
	not_null<Main::Account*> account,
	const ServerConfig &server);

} // namespace Core
