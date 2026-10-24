// ePub export, Wi-Fi and sending books to an Xteink X3 running CrossPoint.
#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../editor/storage.h"
#include "../export/epub.h"
#include "../net/x3.h"
#include "app.h"
#include "pico/stdlib.h"

namespace {
const char* const kWifiPath = "/.fwedit/wifi.txt";
constexpr uint32_t kWifiIdleMs = 10 * 60 * 1000;  // radio off after 10 idle minutes
constexpr uint32_t kConnectMs = 20000;

uint32_t now() { return to_ms_since_boot(get_absolute_time()); }

std::string stem(const std::string& name) {
    const size_t dot = name.rfind('.');
    return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}
}  // namespace

// Shows `what` in the status line; returns a callback that reports false
// once Esc is pressed.
net::KeepGoing App::escChecker(const char* what) {
    msg_ = std::string(what) + "  (Esc cancels)";
    msgUntil_ = now() + 120000;
    renderStatus(true);
    return [this] {
        KeyEvent ev;
        return !(kbd_.poll(ev) && ev.code == key::Esc);
    };
}

// ---------------------------------------------------------------------------
// ePub export
// ---------------------------------------------------------------------------

bool App::exportEpub(std::string* outPath) {
    const std::string docPath = tb_.path().empty() ? "untitled.txt" : tb_.path();
    epub::Options o;
    o.kind = epub::kindForPath(docPath);
    o.author = set_.author;
    o.language = set_.lang.empty() ? "en" : set_.lang;
    const epub::Reader rd = [this](uint32_t pos, uint8_t* buf, uint32_t n) { return tb_.read(pos, buf, n); };
    std::string title = epub::guessTitle(rd, tb_.size(), o.kind, stem(docName()));
    const bool ok = dlg::prompt(ui_, "ePub title", title, "Enter = export   Esc = cancel");
    relayout();
    if (!ok) return false;
    o.title = title.empty() ? stem(docName()) : title;

    const std::string dir = tb_.path().empty() ? set_.cwd : dlg::parent(tb_.path());
    const std::string name = epub::safeFileName(tb_.path().empty() ? o.title : stem(docName())) + ".epub";
    const std::string out = dlg::join(dir, name);
    std::string err;
    const bool written = epub::write(rd, tb_.size(), o, out, &err, progressFn("Exporting", true));
    msg_.clear();
    if (!written) {
        dlg::message(ui_, "Export failed", {err});
        relayout();
        return false;
    }
    *outPath = out;
    return true;
}

void App::cmdExportEpub() {
    std::string out;
    if (exportEpub(&out)) setMessage("Exported " + out, 4000);
}

// ---------------------------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------------------------

void App::loadWifi() {
    fs::File f;
    if (!f.open(kWifiPath, fs::Mode::Read)) return;
    char buf[256];
    const int n = f.read(buf, sizeof buf - 1);
    if (n <= 0) return;
    buf[n] = 0;
    for (char* ln = strtok(buf, "\n"); ln; ln = strtok(nullptr, "\n")) {
        char* eq = strchr(ln, '=');
        if (!eq) continue;
        *eq = 0;
        std::string v = eq + 1;
        if (!v.empty() && v.back() == '\r') v.pop_back();
        if (!strcmp(ln, "ssid")) wifiSsid_ = v;
        else if (!strcmp(ln, "pass")) wifiPass_ = v;
    }
}

void App::saveWifi() {
    const std::string s = "ssid=" + wifiSsid_ + "\npass=" + wifiPass_ + "\n";
    fs::File f;
    if (f.open(kWifiPath, fs::Mode::Write)) f.write(s.data(), uint32_t(s.size()));
}

void App::wifiUsed() { wifiOffAt_ = now() + kWifiIdleMs; }

void App::checkWifiIdle() {
    if (int32_t(now() - wifiOffAt_) < 0) return;
    net::stop();
    statusCache_.clear();
    setMessage("Wi-Fi turned off (idle)");
}

bool App::chooseNetwork() {
    std::vector<net::Network> nets;
    std::string err;
    const bool scanned = net::scan(nets, escChecker("Scanning for Wi-Fi networks..."), &err);
    msg_.clear();
    if (!scanned) {
        dlg::message(ui_, "Wi-Fi", {"Scan failed: " + err});
        relayout();
        return false;
    }
    // Labels must outlive the menu: build all strings first, then point at them.
    std::vector<std::string> right(nets.size());
    for (size_t i = 0; i < nets.size(); ++i) {
        char r[16];
        snprintf(r, sizeof r, "%s%d", nets[i].secure ? "* " : "", nets[i].rssi);
        right[i] = r;
    }
    std::vector<pcui::MenuItem> items;
    for (size_t i = 0; i < nets.size(); ++i) items.push_back(pcui::action(nets[i].ssid.c_str(), right[i].c_str()));
    items.push_back(pcui::action("Other network..."));
    const int r = dlg::menu(ui_, "Wi-Fi networks  (* = password)", items);
    relayout();
    if (r < 0) return false;
    std::string ssid, pass;
    bool secure = true;
    if (r < int(nets.size())) {
        ssid = nets[size_t(r)].ssid;
        secure = nets[size_t(r)].secure;
    } else {
        ssid = wifiSsid_;
        const bool ok = dlg::prompt(ui_, "Network name (SSID)", ssid);
        relayout();
        if (!ok || ssid.empty()) return false;
    }
    if (secure) {
        pass = ssid == wifiSsid_ ? wifiPass_ : "";
        const bool ok = dlg::prompt(ui_, ("Password for " + ssid).c_str(), pass,
                                    "Enter = connect  Tab = show  Esc", true);
        relayout();
        if (!ok) return false;
    }
    net::disconnect();
    const bool ok = net::connect(ssid, pass, kConnectMs, escChecker(("Connecting to " + ssid + "...").c_str()), &err);
    msg_.clear();
    if (!ok) {
        dlg::message(ui_, "Wi-Fi", {"Could not connect to " + ssid + ":", err});
        relayout();
        return false;
    }
    wifiSsid_ = ssid;
    wifiPass_ = pass;
    saveWifi();
    wifiUsed();
    statusCache_.clear();
    setMessage("Connected to " + ssid + "  " + net::ipAddress(), 4000);
    return true;
}

bool App::ensureWifi() {
    if (net::connected()) {
        wifiUsed();
        return true;
    }
    if (!wifiSsid_.empty()) {
        std::string err;
        const bool ok = net::connect(wifiSsid_, wifiPass_, kConnectMs,
                                     escChecker(("Connecting to " + wifiSsid_ + "...").c_str()), &err);
        msg_.clear();
        if (ok) {
            wifiUsed();
            statusCache_.clear();
            return true;
        }
        if (err == "Cancelled") return false;
        const int r = dlg::confirm(ui_, "Wi-Fi", "Could not connect to " + wifiSsid_ + " (" + err +
                                                     "). Choose another network?");
        relayout();
        if (r != 1) return false;
    }
    return chooseNetwork();
}

void App::cmdWifi() {
    for (;;) {
        const bool on = net::connected();
        std::string state = on ? "Connected: " + wifiSsid_ : (wifiSsid_.empty() ? "Not set up" : "Off (" + wifiSsid_ + ")");
        std::string info;
        if (on) {
            char b[48];
            snprintf(b, sizeof b, "%s  %d dBm", net::ipAddress().c_str(), net::rssi());
            info = b;
        }
        // The first line (and, when connected, the address line) are status
        // information: disabled items are shown dimmed and skipped by the cursor.
        std::vector<pcui::MenuItem> items = {
            pcui::action(state.c_str()),
            on ? pcui::action(info.c_str()) : pcui::action("Connect", "Enter"),
            pcui::action("Choose network..."),
            pcui::action("Turn Wi-Fi off"),
            pcui::action("Forget saved network"),
        };
        items[0].enabled = false;
        if (on) items[1].enabled = false;
        items[3].enabled = net::isOn();  // radio may be on without a connection
        items[4].enabled = !wifiSsid_.empty();
        const int r = dlg::menu(ui_, "Wi-Fi", items);
        relayout();
        switch (r) {
            case 1:
                if (!on && ensureWifi()) setMessage("Connected  " + net::ipAddress(), 4000);
                return;
            case 2: chooseNetwork(); return;
            case 3:
                net::stop();
                statusCache_.clear();
                setMessage("Wi-Fi off");
                return;
            case 4:
                if (dlg::confirm(ui_, "Wi-Fi", "Forget " + wifiSsid_ + " and its password?", false) == 1) {
                    net::stop();
                    wifiSsid_.clear();
                    wifiPass_.clear();
                    fs::remove(kWifiPath);
                    statusCache_.clear();
                }
                relayout();
                return;
            default: return;
        }
    }
}

// ---------------------------------------------------------------------------
// X3 (CrossPoint)
// ---------------------------------------------------------------------------

namespace {
// Finds the reader: its host name first (mDNS for *.local), then the last
// address it answered from.
bool findReader(const x3::Target& t, const std::string& lastIp, uint32_t* ip, std::string* err,
                const net::KeepGoing& kg) {
    if (net::resolve(t.host, ip, 6000, kg, err)) return true;
    if (*err == "Cancelled" || lastIp.empty()) return false;
    std::string e2;
    return net::resolve(lastIp, ip, 1000, kg, &e2);
}

std::vector<std::string> unreachable(const std::string& url, const std::string& err) {
    return {"Cannot reach the X3 at " + url + " (" + err + ").",
            "On the X3 open File Transfer and join the same Wi-Fi as the PicoCalc.",
            "If it still fails, set the IP shown on the X3 in ePub & X3 settings."};
}
}  // namespace

void App::cmdSendToX3() {
    x3::Target target;
    std::string err;
    if (!x3::parseUrl(set_.x3Url, &target, &err)) {
        dlg::message(ui_, "Send to X3", {"Bad X3 address: " + err, "Fix it in ePub & X3 settings."});
        relayout();
        return;
    }
    std::string path;
    if (!exportEpub(&path)) return;
    if (!ensureWifi()) {
        setMessage("ePub saved to " + path + " (not sent)", 4000);
        return;
    }
    uint32_t ip = 0;
    const bool found = findReader(target, set_.x3Ip, &ip, &err, escChecker("Looking for the X3..."));
    msg_.clear();
    if (!found) {
        if (err != "Cancelled") dlg::message(ui_, "Send to X3", unreachable(set_.x3Url, err));
        relayout();
        return;
    }

    fs::File f;
    if (!f.open(path.c_str(), fs::Mode::Read)) {
        dlg::message(ui_, "Send to X3", {"Cannot reopen " + path});
        relayout();
        return;
    }
    const uint32_t size = f.size();
    const x3::Source src = [&f](uint32_t off, uint8_t* buf, uint32_t n) -> int {
        if (!f.seek(off)) return -1;
        return f.read(buf, n);
    };
    const std::string name = dlg::baseName(path);
    std::string dir = set_.x3Dir.empty() ? "/" : set_.x3Dir;
    if (dir[0] != '/') dir = "/" + dir;
    x3::Client client(target, ip);
    x3::Reply reply;
    bool ok = client.upload(dir, name, size, src, &reply, progressFn("Sending", true), &err);
    if (ok && x3::isAlreadyExists(reply)) {
        msg_.clear();
        const int r = dlg::confirm(ui_, "Send to X3", name + " is already on the X3. Replace it?", false);
        relayout();
        if (r != 1) {
            setMessage("Not sent: already on the X3");
            return;
        }
        const std::string remote = (dir == "/" ? "" : dir) + "/" + name;
        if (!client.remove(remote, &reply, escChecker("Replacing..."), &err)) ok = false;
        else ok = client.upload(dir, name, size, src, &reply, progressFn("Sending", true), &err);
    }
    msg_.clear();
    wifiUsed();
    if (!ok) {
        if (err == "Cancelled") setMessage("Sending cancelled");
        else dlg::message(ui_, "Send to X3", unreachable(set_.x3Url, err));
        relayout();
        return;
    }
    if (reply.status != 200) {
        char s[16];
        snprintf(s, sizeof s, "%d", reply.status);
        dlg::message(ui_, "Send to X3", {std::string("The X3 answered ") + s + ":", reply.body});
        relayout();
        return;
    }
    const std::string ipStr = net::ipToString(ip);
    if (ipStr != set_.x3Ip) {
        set_.x3Ip = ipStr;
        saveSettings();
    }
    setMessage("Sent to X3: " + (dir == "/" ? "" : dir) + "/" + name, 5000);
}

void App::cmdShareSettings() {
    int sel = 0;
    for (;;) {
        const std::string a = "Author: " + (set_.author.empty() ? std::string("-") : set_.author);
        const std::string l = "Book language: " + set_.lang;
        const std::string u = "X3 address: " + set_.x3Url;
        const std::string d = "X3 folder: " + set_.x3Dir;
        const std::vector<pcui::MenuItem> items = {
            pcui::action(a.c_str()), pcui::action(l.c_str()), pcui::action(u.c_str()),
            pcui::action(d.c_str()), pcui::action("Test X3 connection"),
        };
        pcui::MenuOptions opt;
        opt.initial = sel;
        sel = dlg::menu(ui_, "ePub & X3 settings", items, opt);
        relayout();
        if (sel < 0) return;
        std::string v;
        switch (sel) {
            case 0:
                v = set_.author;
                if (dlg::prompt(ui_, "Author (ePub metadata)", v)) set_.author = v;
                break;
            case 1:
                v = set_.lang;
                if (dlg::prompt(ui_, "Book language (en, it, de...)", v) && !v.empty()) set_.lang = v;
                break;
            case 2:
                v = set_.x3Url;
                if (dlg::prompt(ui_, "X3 address (URL or IP)", v, "Empty = http://crosspoint.local/")) {
                    x3::Target t;
                    std::string err;
                    if (v.empty()) v = x3::kDefaultUrl;
                    if (x3::parseUrl(v, &t, &err)) {
                        set_.x3Url = v;
                        set_.x3Ip.clear();
                    } else {
                        dlg::message(ui_, "X3 address", {err});
                    }
                }
                break;
            case 3:
                v = set_.x3Dir;
                if (dlg::prompt(ui_, "Folder on the X3", v, "e.g. / or /Books")) set_.x3Dir = v.empty() ? "/" : v;
                break;
            case 4: {
                x3::Target t;
                std::string err;
                std::vector<std::string> lines;
                uint32_t ip = 0;
                x3::Reply r;
                if (!x3::parseUrl(set_.x3Url, &t, &err)) {
                    lines = {"Bad address: " + err};
                } else if (!ensureWifi()) {
                    break;
                } else if (!findReader(t, set_.x3Ip, &ip, &err, escChecker("Looking for the X3..."))) {
                    lines = unreachable(set_.x3Url, err);
                } else if (!x3::Client(t, ip).status(&r, escChecker("Asking the X3..."), &err) ||
                           r.status != 200) {
                    lines = unreachable(set_.x3Url, err.empty() ? "no answer" : err);
                } else {
                    lines = {"Found at " + net::ipToString(ip) + ":", r.body};
                    if (set_.x3Ip != net::ipToString(ip)) set_.x3Ip = net::ipToString(ip);
                }
                msg_.clear();
                wifiUsed();
                dlg::message(ui_, "Test X3 connection", lines);
                break;
            }
        }
        saveSettings();
        relayout();
    }
}
