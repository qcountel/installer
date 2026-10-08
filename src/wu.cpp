// Compiled without the precompiled header.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "wu.h"
#include "net.h"

namespace {

const char* SECURED_URL = "https://fe3.delivery.mp.microsoft.com/ClientWebService/client.asmx/secured";
const char* CDN_PREFIX  = "http://tlu.dl.delivery.mp.microsoft.com/";

// Device attributes of a regular Windows 10 desktop, copied from MCLauncher (XML-escaped).
const char* DEVICE_ATTRIBUTES =
    "E:BranchReadinessLevel=CBB&amp;DchuNvidiaGrfxExists=1&amp;ProcessorIdentifier=Intel64%20Family%206%20Model%2063%20Stepping%202"
    "&amp;CurrentBranch=rs4_release&amp;DataVer_RS5=1942&amp;FlightRing=Retail&amp;AttrDataVer=57&amp;InstallLanguage=en-US"
    "&amp;DchuAmdGrfxExists=1&amp;OSUILocale=en-US&amp;InstallationType=Client&amp;FlightingBranchName=&amp;Version_RS5=10"
    "&amp;UpgEx_RS5=Green&amp;GStatus_RS5=2&amp;OSSkuId=48&amp;App=WU&amp;InstallDate=1529700913&amp;ProcessorManufacturer=GenuineIntel"
    "&amp;AppVer=10.0.17134.471&amp;OSArchitecture=AMD64&amp;UpdateManagementGroup=2&amp;IsDeviceRetailDemo=0"
    "&amp;HidOverGattReg=C%3A%5CWINDOWS%5CSystem32%5CDriverStore%5CFileRepository%5Chidbthle.inf_amd64_467f181075371c89%5CMicrosoft.Bluetooth.Profiles.HidOverGatt.dll"
    "&amp;IsFlightingEnabled=0&amp;DchuIntelGrfxExists=1&amp;TelemetryLevel=1&amp;DefaultUserRegion=244&amp;DeferFeatureUpdatePeriodInDays=365"
    "&amp;Bios=Unknown&amp;WuClientVer=10.0.17134.471&amp;PausedFeatureStatus=1&amp;Steam=URL%3Asteam%20protocol&amp;Free=8to16"
    "&amp;OSVersion=10.0.17134.472&amp;DeviceFamily=Windows.Desktop";

std::string IsoTime(const FILETIME& ft) {
    SYSTEMTIME st{};
    FileTimeToSystemTime(&ft, &st);
    char buf[40];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02uT%02u:%02u:%02u.000Z",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

// Only GUID characters are allowed: the id is pasted into XML.
bool IsGuid(const std::string& s) {
    if (s.size() != 36) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        bool dash = (i == 8 || i == 13 || i == 18 || i == 23);
        if (dash ? c != '-' : !isxdigit((unsigned char)c)) return false;
    }
    return true;
}

std::string BuildRequest(const std::string& updateId) {
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER t{ { now.dwLowDateTime, now.dwHighDateTime } };
    t.QuadPart += 5ull * 60 * 10000000;   // +5 minutes
    FILETIME expires{ t.LowPart, t.HighPart };

    std::string x;
    x += "<s:Envelope xmlns:a=\"http://www.w3.org/2005/08/addressing\" xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">";
    x += "<s:Header>";
    x += "<a:Action s:mustUnderstand=\"1\">http://www.microsoft.com/SoftwareDistribution/Server/ClientWebService/GetExtendedUpdateInfo2</a:Action>";
    x += "<a:MessageID>urn:uuid:5754a03d-d8d5-489f-b24d-efc31b3fd32d</a:MessageID>";
    x += std::string("<a:To s:mustUnderstand=\"1\">") + SECURED_URL + "</a:To>";
    x += "<o:Security s:mustUnderstand=\"1\" xmlns:o=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\">";
    x += "<Timestamp xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">";
    x += "<Created>" + IsoTime(now) + "</Created><Expires>" + IsoTime(expires) + "</Expires></Timestamp>";
    x += "<wuws:WindowsUpdateTicketsToken wsu:id=\"ClientMSA\" "
         "xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\" "
         "xmlns:wuws=\"http://schemas.microsoft.com/msus/2014/10/WindowsUpdateAuthorization\">";
    x += "<TicketType Name=\"AAD\" Version=\"1.0\" Policy=\"MBI_SSL\"></TicketType>";
    x += "</wuws:WindowsUpdateTicketsToken></o:Security></s:Header>";
    x += "<s:Body><GetExtendedUpdateInfo2 xmlns=\"http://www.microsoft.com/SoftwareDistribution/Server/ClientWebService\">";
    x += "<updateIDs><UpdateIdentity><UpdateID>" + updateId + "</UpdateID><RevisionNumber>1</RevisionNumber></UpdateIdentity></updateIDs>";
    x += "<infoTypes><XmlUpdateFragmentType>FileUrl</XmlUpdateFragmentType></infoTypes>";
    x += std::string("<deviceAttributes>") + DEVICE_ATTRIBUTES + "</deviceAttributes>";
    x += "</GetExtendedUpdateInfo2></s:Body></s:Envelope>";
    return x;
}

std::string XmlUnescape(std::string s) {
    static const std::pair<const char*, const char*> map[] = {
        { "&lt;", "<" }, { "&gt;", ">" }, { "&quot;", "\"" }, { "&apos;", "'" }, { "&amp;", "&" } };
    for (auto& [from, to] : map) {
        size_t pos = 0, n = strlen(from);
        while ((pos = s.find(from, pos)) != std::string::npos) { s.replace(pos, n, to); pos += strlen(to); }
    }
    return s;
}

// Collects the text of every <...Url>...</...Url> element (namespace prefixes are ignored).
std::vector<std::string> ExtractUrls(const std::string& xml) {
    std::vector<std::string> urls;
    size_t pos = 0;
    while ((pos = xml.find("Url>", pos)) != std::string::npos) {
        size_t lt = xml.rfind('<', pos);
        pos += 4;
        if (lt == std::string::npos || xml[lt + 1] == '/') continue;   // closing tag
        size_t end = xml.find("</", pos);
        if (end == std::string::npos) break;
        urls.push_back(XmlUnescape(xml.substr(pos, end - pos)));
        pos = end;
    }
    return urls;
}

} // namespace

namespace WU {

bool ResolveDownloadUrl(const std::wstring& updateIdW, std::wstring& url, std::wstring& status) {
    std::string updateId = Net::Narrow(updateIdW);
    if (!IsGuid(updateId)) { status = L"неверный идентификатор версии"; return false; }

    std::string response;
    if (!Net::Post(Net::Widen(SECURED_URL), BuildRequest(updateId), L"application/soap+xml; charset=utf-8",
                   response, status)) {
        status = L"Сервер обновлений Microsoft: " + status;
        return false;
    }

    for (const std::string& u : ExtractUrls(response)) {
        if (u.rfind(CDN_PREFIX, 0) == 0) {
            url = Net::Widen(u);
            return true;
        }
    }
    status = L"Microsoft не выдал ссылку на эту версию (её могли убрать из Store)";
    return false;
}

} // namespace WU
