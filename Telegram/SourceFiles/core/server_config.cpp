/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/server_config.h"

#include "base/algorithm.h"
#include "main/main_account.h"
#include "mtproto/mtp_instance.h"
#include "mtproto/mtproto_config.h"
#include "mtproto/mtproto_dc_options.h"
#include "storage/storage_account.h"

#include <QtCore/QDataStream>
#include <QtCore/QIODevice>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtNetwork/QNetworkProxy>

namespace Core {
namespace {

constexpr auto kServerBindingKey = "baigram_server";

[[nodiscard]] QByteArray SerializeBinding(const ServerConfig &config) {
	auto result = QByteArray();
	QDataStream stream(&result, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_5_1);
	stream << config.id << config.ip << config.port << config.pubkey
		<< config.name << config.systrayLight << config.systrayDark
		<< config.deeplink << config.dflink << config.website
		<< config.changelogLink << config.aboutText;
	return result;
}

[[nodiscard]] std::optional<ServerConfig> DeserializeBinding(
		const QByteArray &data) {
	auto stream = QDataStream(data);
	stream.setVersion(QDataStream::Qt_5_1);
	auto config = ServerConfig();
	stream >> config.id >> config.ip >> config.port >> config.pubkey;
	if (stream.status() != QDataStream::Ok) {
		return std::nullopt;
	}
	if (!stream.atEnd()) {
		stream >> config.name;
	}
	if (!stream.atEnd()) {
		stream >> config.systrayLight;
	}
	if (!stream.atEnd()) {
		stream >> config.systrayDark;
	}
	if (!stream.atEnd()) {
		stream >> config.deeplink;
	}
	if (!stream.atEnd()) {
		stream >> config.dflink;
	}
	if (!stream.atEnd()) {
		stream >> config.website;
	}
	if (!stream.atEnd()) {
		stream >> config.changelogLink;
	}
	if (!stream.atEnd()) {
		stream >> config.aboutText;
	}
	return (stream.status() == QDataStream::Ok)
		? std::make_optional(config)
		: std::nullopt;
}

void ApplyToDcOptions(MTP::DcOptions &dc, const ServerConfig &config) {
	if (!config.validDc()) {
		return;
	}
	const auto flags = MTP::DcOptions::Flag::f_static | 0;
	dc.constructReplaceOne(
		MTP::Instance::Fields::kDefaultMainDc,
		flags,
		config.ip.toStdString(),
		config.port,
		{});
	if (config.hasPubKey()) {
		const auto pem = config.pubkey.toUtf8();
		dc.constructAddPublicKey(bytes::make_span(pem));
	}
	dc.setImmutable();
}

} // namespace

ServerConfigManager::ServerConfigManager()
: _current(ServerConfig()) {
}

ServerConfigManager &ServerConfigManager::Instance() {
	static ServerConfigManager instance;
	return instance;
}

void ServerConfigManager::request(
		const QString &id,
		Fn<void(const ServerConfig&)> onDone,
		Fn<void(const QString&)> onFail) {
	cancel();

	_manager.setProxy(QNetworkProxy::NoProxy);
	auto request = QNetworkRequest(QUrl(
		u"https://server-worker.pages.dev/getserver?id=%1"_q.arg(id)));
	request.setTransferTimeout(10000);
	const auto reply = _manager.get(request);
	_reply = reply;

	connect(reply, &QNetworkReply::finished, this, [=] {
		const auto taken = base::take(_reply);
		if (!taken) {
			return;
		}
		if (taken->error() != QNetworkReply::NoError) {
			const auto error = taken->errorString();
			taken->deleteLater();
			if (onFail) {
				onFail(error);
			}
			return;
		}
		const auto bytes = taken->readAll();
		taken->deleteLater();

		auto error = QJsonParseError();
		const auto doc = QJsonDocument::fromJson(bytes, &error);
		if (error.error != QJsonParseError::NoError || !doc.isObject()) {
			if (onFail) {
				onFail(u"bad response"_q);
			}
			return;
		}
		const auto obj = doc.object();
		if (obj.value(u"code"_q).toInt() != 0) {
			if (onFail) {
				onFail(obj.value(u"msg"_q).toString(u"server error"_q));
			}
			return;
		}
		auto config = ServerConfig();
		config.id = obj.value(u"query"_q).toString(id);
		config.ip = obj.value(u"ip"_q).toString();
		config.name = obj.value(u"name"_q).toString();
		config.port = obj.value(u"port"_q).toInt();
		config.pubkey = obj.value(u"pubkey"_q).toString();
		config.systrayLight = obj.value(u"systray_light"_q).toString();
		config.systrayDark = obj.value(u"systray_dark"_q).toString();
		config.deeplink = obj.value(u"deeplink"_q).toString();
		config.dflink = obj.value(u"dflink"_q).toString();
		config.website = obj.value(u"website"_q).toString();
		config.changelogLink = obj.value(u"changelog_link"_q).toString();
		config.aboutText = obj.value(u"about_text"_q).toString();

		_current = config;
		if (onDone) {
			onDone(config);
		}
	});
}

void ServerConfigManager::cancel() {
	if (const auto reply = base::take(_reply)) {
		reply->abort();
		reply->deleteLater();
	}
}

void ServerConfigManager::resetToDefault() {
	cancel();
	_current = ServerConfig();
}

void ServerConfigManager::setCurrent(const ServerConfig &config) {
	_current = config;
}

void SaveServerBinding(
		not_null<Main::Account*> account,
		const ServerConfig &config) {
	account->local().writePrefGeneric(
		kServerBindingKey,
		SerializeBinding(config));
}

std::optional<ServerConfig> ReadServerBinding(
		not_null<Main::Account*> account) {
	const auto data = account->local().readPrefGeneric(kServerBindingKey);
	return data ? DeserializeBinding(*data) : std::nullopt;
}

void ApplyServerBindingToConfig(
		MTP::Config &config,
		const ServerConfig &server) {
	ApplyToDcOptions(config.dcOptions(), server);
}

void ApplyServerBindingToAccount(
		not_null<Main::Account*> account,
		const ServerConfig &server) {
	auto &mtp = account->mtp();
	ApplyToDcOptions(mtp.dcOptions(), server);
	mtp.reInitConnection(MTP::Instance::Fields::kDefaultMainDc);
	mtp.requestConfig();
}

} // namespace Core
