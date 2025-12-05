/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/

#include "platform/platform_webauthn.h"

#include "data/data_passkey_deserialize.h"

#include <windows.h>
#include <combaseapi.h>
#include <webauthn.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QWindow>
#include <QGuiApplication>

namespace Platform::WebAuthn {
namespace {

auto webauthn = (HMODULE)(nullptr);

using IsAvailableFunc = HRESULT(WINAPI*)(
	BOOL *pbIsUserVerifyingPlatformAuthenticatorAvailable);
using MakeCredentialFunc = HRESULT(WINAPI*)(
	HWND hWnd,
	PCWEBAUTHN_RP_ENTITY_INFORMATION pRpInformation,
	PCWEBAUTHN_USER_ENTITY_INFORMATION pUserInformation,
	PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS pPubKeyCredParams,
	PCWEBAUTHN_CLIENT_DATA pWebAuthNClientData,
	PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS
		pWebAuthNCredentialOptions,
	PWEBAUTHN_CREDENTIAL_ATTESTATION *ppWebAuthNCredentialAttestation);
using FreeAttestationFunc = void(WINAPI*)(
	PWEBAUTHN_CREDENTIAL_ATTESTATION pWebAuthNCredentialAttestation);
using GetAssertionFunc = HRESULT(WINAPI*)(
	HWND hWnd,
	LPCWSTR pwszRpId,
	PCWEBAUTHN_CLIENT_DATA pWebAuthNClientData,
	PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS
		pWebAuthNGetAssertionOptions,
	PWEBAUTHN_ASSERTION *ppWebAuthNAssertion);
using FreeAssertionFunc = void(WINAPI*)(
	PWEBAUTHN_ASSERTION pWebAuthNAssertion);
using GetVersionFunc = DWORD(WINAPI*)();
using GetErrorNameFunc = PCWSTR(WINAPI*)(HRESULT hr);
using CancelOperationFunc = HRESULT(WINAPI*)(GUID* pCancellationId);

auto WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable
	= (IsAvailableFunc)(nullptr);
auto WebAuthNAuthenticatorMakeCredential = (MakeCredentialFunc)(nullptr);
auto WebAuthNFreeCredentialAttestation = (FreeAttestationFunc)(nullptr);
auto WebAuthNAuthenticatorGetAssertion = (GetAssertionFunc)(nullptr);
auto WebAuthNFreeAssertion = (FreeAssertionFunc)(nullptr);
auto WebAuthNGetApiVersionNumber = (GetVersionFunc)(nullptr);
auto WebAuthNGetErrorName = (GetErrorNameFunc)(nullptr);
auto WebAuthNCancelCurrentOperation = (CancelOperationFunc)(nullptr);

DWORD apiVersion = 0;

bool LoadWebAuthn() {
	if (webauthn) {
		return true;
	}
	webauthn = LoadLibraryExW(
		L"webauthn.dll",
		nullptr,
		LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!webauthn) {
		return false;
	}

	WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable =
		(IsAvailableFunc)GetProcAddress(
			webauthn,
			"WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable");
	WebAuthNAuthenticatorMakeCredential =
		(MakeCredentialFunc)GetProcAddress(
			webauthn,
			"WebAuthNAuthenticatorMakeCredential");
	WebAuthNFreeCredentialAttestation =
		(FreeAttestationFunc)GetProcAddress(
			webauthn,
			"WebAuthNFreeCredentialAttestation");
	WebAuthNAuthenticatorGetAssertion =
		(GetAssertionFunc)GetProcAddress(
			webauthn,
			"WebAuthNAuthenticatorGetAssertion");
	WebAuthNFreeAssertion =
		(FreeAssertionFunc)GetProcAddress(
			webauthn,
			"WebAuthNFreeAssertion");

	WebAuthNGetApiVersionNumber =
		(GetVersionFunc)GetProcAddress(
			webauthn,
			"WebAuthNGetApiVersionNumber");
	WebAuthNGetErrorName =
		(GetErrorNameFunc)GetProcAddress(
			webauthn,
			"WebAuthNGetErrorName");
	WebAuthNCancelCurrentOperation =
		(CancelOperationFunc)GetProcAddress(
			webauthn,
			"WebAuthNCancelCurrentOperation");

	apiVersion = WebAuthNGetApiVersionNumber
		? WebAuthNGetApiVersionNumber()
		: 0;

	return WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable
		&& WebAuthNAuthenticatorMakeCredential
		&& WebAuthNFreeCredentialAttestation
		&& WebAuthNAuthenticatorGetAssertion
		&& WebAuthNFreeAssertion;
}

} // namespace

bool IsSupported() {
	if (!LoadWebAuthn()) {
		return false;
	}
	auto available = (BOOL)(FALSE);
	return SUCCEEDED(
		WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable(&available))
			&& available;
}

void RegisterKey(
		const Data::Passkey::RegisterData &data,
		Fn<void(RegisterResult result)> callback) {
	if (!LoadWebAuthn()) {
		callback({});
		return;
	}

	auto rpId = data.rp.id.toStdWString();
	auto rpName = data.rp.name.toStdWString();
	auto userName = data.user.name.toStdWString();
	auto userDisplayName = data.user.displayName.toStdWString();

	auto rpInfo = WEBAUTHN_RP_ENTITY_INFORMATION();
	memset(&rpInfo, 0, sizeof(rpInfo));
	rpInfo.dwVersion =
		WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION;
	rpInfo.pwszId = rpId.c_str();
	rpInfo.pwszName = rpName.c_str();

	auto userInfo = WEBAUTHN_USER_ENTITY_INFORMATION();
	memset(&userInfo, 0, sizeof(userInfo));
	userInfo.dwVersion =
		WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION;
	userInfo.cbId = data.user.id.size();
	userInfo.pbId = (PBYTE)data.user.id.data();
	userInfo.pwszName = userName.c_str();
	userInfo.pwszDisplayName = userDisplayName.c_str();

	auto credParams = std::vector<WEBAUTHN_COSE_CREDENTIAL_PARAMETER>();
	for (const auto &param : data.pubKeyCredParams) {
		auto cp = WEBAUTHN_COSE_CREDENTIAL_PARAMETER{};
		cp.dwVersion =
			WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION;
		auto type = param.type.toStdWString();
		cp.pwszCredentialType = type == L"public-key"
			? WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY
			: L"";
		cp.lAlg = param.alg;
		credParams.push_back(cp);
	}

	auto credParamsList = WEBAUTHN_COSE_CREDENTIAL_PARAMETERS();
	memset(&credParamsList, 0, sizeof(credParamsList));
	credParamsList.cCredentialParameters = credParams.size();
	credParamsList.pCredentialParameters = credParams.data();

	auto clientDataJson = Data::Passkey::SerializeClientDataCreate(
		data.challenge);
	auto clientData = WEBAUTHN_CLIENT_DATA();
	memset(&clientData, 0, sizeof(clientData));
	clientData.dwVersion = WEBAUTHN_CLIENT_DATA_CURRENT_VERSION;
	clientData.cbClientDataJSON = clientDataJson.size();
	clientData.pbClientDataJSON = (PBYTE)clientDataJson.data();
	clientData.pwszHashAlgId = WEBAUTHN_HASH_ALGORITHM_SHA_256;

	auto cancellationId = GUID();
	CoCreateGuid(&cancellationId);

	auto options = WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS();
	memset(&options, 0, sizeof(options));
	options.dwVersion =
		WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_CURRENT_VERSION;
	options.dwTimeoutMilliseconds = data.timeout;
	options.dwAuthenticatorAttachment =
		WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;
	options.bRequireResidentKey = FALSE;
	options.dwUserVerificationRequirement =
		WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
	options.dwAttestationConveyancePreference =
		WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE;
	options.pCancellationId = &cancellationId;

#if defined(WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_4) \
	|| defined(WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_5) \
	|| defined(WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_6) \
	|| defined(WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_7) \
	|| defined(WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_8) \
	|| defined(WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_9)
	options.bPreferResidentKey = FALSE;
#endif

	auto hwnd = (HWND)(nullptr);
	if (auto window = QGuiApplication::topLevelWindows().value(0)) {
		hwnd = (HWND)window->winId();
		if (hwnd) {
			SetForegroundWindow(hwnd);
			SetFocus(hwnd);
		}
	}

	auto attestation = (PWEBAUTHN_CREDENTIAL_ATTESTATION)(nullptr);
	auto hr = (HRESULT)(WebAuthNAuthenticatorMakeCredential)(
		hwnd,
		&rpInfo,
		&userInfo,
		&credParamsList,
		&clientData,
		&options,
		&attestation);

	if (SUCCEEDED(hr) && attestation) {
		auto result = RegisterResult();
		result.success = true;
		result.credentialId = QByteArray(
			(char*)attestation->pbCredentialId,
			attestation->cbCredentialId);
		result.attestationObject = QByteArray(
			(char*)attestation->pbAttestationObject,
			attestation->cbAttestationObject);
		result.clientDataJSON = QByteArray::fromStdString(clientDataJson);
		WebAuthNFreeCredentialAttestation(attestation);
		callback(result);
	} else {
		callback({});
	}
}

void Login(
		const Data::Passkey::LoginData &data,
		Fn<void(LoginResult result)> callback) {
	if (!LoadWebAuthn()) {
		callback({});
		return;
	}

	auto rpId = data.rpId.toStdWString();
	auto clientDataJson = Data::Passkey::SerializeClientDataGet(
		data.challenge);

	auto clientData = WEBAUTHN_CLIENT_DATA();
	memset(&clientData, 0, sizeof(clientData));
	clientData.dwVersion = WEBAUTHN_CLIENT_DATA_CURRENT_VERSION;
	clientData.cbClientDataJSON = clientDataJson.size();
	clientData.pbClientDataJSON = (PBYTE)clientDataJson.data();
	clientData.pwszHashAlgId = WEBAUTHN_HASH_ALGORITHM_SHA_256;

	auto allowCredentials = std::vector<WEBAUTHN_CREDENTIAL>();
	auto credentialIds = std::vector<QByteArray>();
	for (const auto &cred : data.allowCredentials) {
		credentialIds.push_back(cred.id);
		auto credential = WEBAUTHN_CREDENTIAL();
		memset(&credential, 0, sizeof(credential));
		credential.dwVersion = WEBAUTHN_CREDENTIAL_CURRENT_VERSION;
		credential.cbId = cred.id.size();
		credential.pbId = (PBYTE)credentialIds.back().data();
		credential.pwszCredentialType = WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY;
		allowCredentials.push_back(credential);
	}

	auto cancellationId = GUID();
	CoCreateGuid(&cancellationId);

	auto options = WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS();
	memset(&options, 0, sizeof(options));
	options.dwVersion =
		WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_CURRENT_VERSION;
	options.dwTimeoutMilliseconds = data.timeout;
	if (!allowCredentials.empty()) {
		options.CredentialList.cCredentials = allowCredentials.size();
		options.CredentialList.pCredentials = allowCredentials.data();
	}
	options.pCancellationId = &cancellationId;
	if (data.userVerification == "required") {
		options.dwUserVerificationRequirement =
			WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
	} else if (data.userVerification == "preferred") {
		options.dwUserVerificationRequirement =
			WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
	} else {
		options.dwUserVerificationRequirement =
			WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
	}

	auto hwnd = (HWND)(nullptr);
	if (auto window = QGuiApplication::topLevelWindows().value(0)) {
		hwnd = (HWND)window->winId();
		if (hwnd) {
			SetForegroundWindow(hwnd);
			SetFocus(hwnd);
		}
	}

	auto assertion = (PWEBAUTHN_ASSERTION)(nullptr);
	auto hr = (HRESULT)(WebAuthNAuthenticatorGetAssertion)(
		hwnd,
		rpId.c_str(),
		&clientData,
		&options,
		&assertion);

	if (SUCCEEDED(hr) && assertion) {
		auto result = LoginResult();
		result.clientDataJSON = QByteArray::fromStdString(clientDataJson);
		result.credentialId = QByteArray(
			(char*)assertion->Credential.pbId,
			assertion->Credential.cbId);
		result.authenticatorData = QByteArray(
			(char*)assertion->pbAuthenticatorData,
			assertion->cbAuthenticatorData);
		result.signature = QByteArray(
			(char*)assertion->pbSignature,
			assertion->cbSignature);
		result.userHandle = assertion->cbUserId > 0
			? QByteArray((char*)assertion->pbUserId, assertion->cbUserId)
			: QByteArray();
		WebAuthNFreeAssertion(assertion);
		callback(result);
	} else {
		callback({});
	}
}

} // namespace Platform::WebAuthn
