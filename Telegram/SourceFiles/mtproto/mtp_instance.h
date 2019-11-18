/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/mtproto_rpc_sender.h"

namespace MTP {
namespace internal {

class Dcenter;
class Session;
class Connection;

[[nodiscard]] int GetNextRequestId();

} // namespace internal

class DcOptions;
class AuthKey;
using AuthKeyPtr = std::shared_ptr<AuthKey>;
using AuthKeysList = std::vector<AuthKeyPtr>;

class Instance : public QObject {
	Q_OBJECT

public:
	struct Config {
		static constexpr auto kNoneMainDc = -1;
		static constexpr auto kNotSetMainDc = 0;
		static constexpr auto kDefaultMainDc = 2;
		static constexpr auto kTemporaryMainDc = 1000;

		DcId mainDcId = kNotSetMainDc;
		AuthKeysList keys;
		QString deviceModel;
		QString systemVersion;
	};

	enum class Mode {
		Normal,
		KeysDestroyer,
	};

	Instance(not_null<DcOptions*> options, Mode mode, Config &&config);
	Instance(const Instance &other) = delete;
	Instance &operator=(const Instance &other) = delete;
	~Instance();

	void resolveProxyDomain(const QString &host);
	void setGoodProxyDomain(const QString &host, const QString &ip);
	void suggestMainDcId(DcId mainDcId);
	void setMainDcId(DcId mainDcId);
	[[nodiscard]] DcId mainDcId() const;
	[[nodiscard]] QString systemLangCode() const;
	[[nodiscard]] QString cloudLangCode() const;
	[[nodiscard]] QString langPackName() const;

	// Thread-safe.
	[[nodiscard]] QString deviceModel() const;
	[[nodiscard]] QString systemVersion() const;
	void setKeyForWrite(DcId dcId, const AuthKeyPtr &key);

	// Main thread.
	[[nodiscard]] AuthKeysList getKeysForWrite() const;
	void addKeysForDestroy(AuthKeysList &&keys);

	[[nodiscard]] not_null<DcOptions*> dcOptions();

	void restart();
	void restart(ShiftedDcId shiftedDcId);
	int32 dcstate(ShiftedDcId shiftedDcId = 0);
	QString dctransport(ShiftedDcId shiftedDcId = 0);
	void ping();
	void cancel(mtpRequestId requestId);
	int32 state(mtpRequestId requestId); // < 0 means waiting for such count of ms

	// Main thread.
	void killSession(ShiftedDcId shiftedDcId);
	void stopSession(ShiftedDcId shiftedDcId);
	void reInitConnection(DcId dcId);
	void logout(RPCDoneHandlerPtr onDone, RPCFailHandlerPtr onFail);

	void unpaused();

	void queueQuittingConnection(std::unique_ptr<internal::Connection> &&connection);

	void setUpdatesHandler(RPCDoneHandlerPtr onDone);
	void setGlobalFailHandler(RPCFailHandlerPtr onFail);
	void setStateChangedHandler(Fn<void(ShiftedDcId shiftedDcId, int32 state)> handler);
	void setSessionResetHandler(Fn<void(ShiftedDcId shiftedDcId)> handler);
	void clearGlobalHandlers();

	void onStateChange(ShiftedDcId shiftedDcId, int32 state);
	void onSessionReset(ShiftedDcId shiftedDcId);

	void clearCallbacksDelayed(std::vector<RPCCallbackClear> &&ids);

	void execCallback(mtpRequestId requestId, const mtpPrime *from, const mtpPrime *end);
	bool hasCallbacks(mtpRequestId requestId);
	void globalCallback(const mtpPrime *from, const mtpPrime *end);

	// return true if need to clean request data
	bool rpcErrorOccured(mtpRequestId requestId, const RPCFailHandlerPtr &onFail, const RPCError &err);

	bool isKeysDestroyer() const;
	void scheduleKeyDestroy(ShiftedDcId shiftedDcId);
	void checkIfKeyWasDestroyed(ShiftedDcId shiftedDcId);
	void keyDestroyedOnServer(DcId dcId, uint64 keyId);

	void requestConfig();
	void requestConfigIfOld();
	void requestCDNConfig();
	void setUserPhone(const QString &phone);
	void badConfigurationError();

	void syncHttpUnixtime();

	void connectionFinished(not_null<internal::Connection*> connection);

	void sendAnything(ShiftedDcId shiftedDcId = 0, crl::time msCanWait = 0);
	void sendDcKeyCheck(ShiftedDcId shiftedDcId, const AuthKeyPtr &key);

	template <typename Request>
	mtpRequestId send(
			const Request &request,
			RPCResponseHandler &&callbacks = {},
			ShiftedDcId shiftedDcId = 0,
			crl::time msCanWait = 0,
			mtpRequestId afterRequestId = 0) {
		const auto requestId = internal::GetNextRequestId();
		sendSerialized(
			requestId,
			SecureRequest::Serialize(request),
			std::move(callbacks),
			shiftedDcId,
			msCanWait,
			afterRequestId);
		return requestId;
	}

	template <typename Request>
	mtpRequestId send(
			const Request &request,
			RPCDoneHandlerPtr &&onDone,
			RPCFailHandlerPtr &&onFail = nullptr,
			ShiftedDcId shiftedDcId = 0,
			crl::time msCanWait = 0,
			mtpRequestId afterRequestId = 0) {
		return send(
			request,
			RPCResponseHandler(std::move(onDone), std::move(onFail)),
			shiftedDcId,
			msCanWait,
			afterRequestId);
	}

	template <typename Request>
	mtpRequestId sendProtocolMessage(
			ShiftedDcId shiftedDcId,
			const Request &request) {
		const auto requestId = internal::GetNextRequestId();
		sendRequest(
			requestId,
			SecureRequest::Serialize(request),
			{},
			shiftedDcId,
			0,
			false,
			0);
		return requestId;
	}

	void sendSerialized(
			mtpRequestId requestId,
			SecureRequest &&request,
			RPCResponseHandler &&callbacks,
			ShiftedDcId shiftedDcId,
			crl::time msCanWait,
			mtpRequestId afterRequestId) {
		const auto needsLayer = true;
		sendRequest(
			requestId,
			std::move(request),
			std::move(callbacks),
			shiftedDcId,
			msCanWait,
			needsLayer,
			afterRequestId);
	}

signals:
	void configLoaded();
	void cdnConfigLoaded();
	void allKeysDestroyed();
	void proxyDomainResolved(
		QString host,
		QStringList ips,
		qint64 expireAt);

private:
	void sendRequest(
		mtpRequestId requestId,
		SecureRequest &&request,
		RPCResponseHandler &&callbacks,
		ShiftedDcId shiftedDcId,
		crl::time msCanWait,
		bool needsLayer,
		mtpRequestId afterRequestId);

	class Private;
	const std::unique_ptr<Private> _private;

};

} // namespace MTP
