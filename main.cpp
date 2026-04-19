#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 // Set Windows 10 target to satisfy IDE linting
#endif

#include "httplib.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

using namespace httplib;
using namespace std;

// Mock Database
struct User {
    string id;
    string name;
    string role;
};

map<string, User> users = {
    {"1", {"1", "Alice Admin", "Admin"}},
    {"2", {"2", "Bob Manager", "Manager"}},
    {"3", {"3", "Charlie Employee", "Employee"}},
    {"4", {"4", "Dave Employee", "Employee"}}
};

map<string, vector<string>> user_msgs = {
    {"1", {"System alert: Update the server!"}},
    {"2", {"Manager note: Approve team timesheets."}},
    {"3", {"Employee msg: Training at 3 PM."}},
    {"4", {"Employee msg: Submit the Q3 report Friday."}}
};

string get_session_cookie(const Headers& headers) {
    if (headers.count("Cookie")) {
        auto rng = headers.equal_range("Cookie");
        for (auto it = rng.first; it != rng.second; ++it) {
            string cookie = it->second;
            size_t pos = cookie.find("session_user_id=");
            if (pos != string::npos) {
                size_t start = pos + 16;
                size_t end = cookie.find(";", start);
                return cookie.substr(start, end - start);
            }
        }
    }
    return "";
}

const string GLOBAL_CSS_STYLE = R"(<style>
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap');
    body { font-family: 'Inter', sans-serif; line-height: 1.6; max-width: 800px; margin: 40px auto; padding: 20px; background: linear-gradient(135deg, #0f172a 0%, #1e1b4b 100%); color: #e2e8f0; min-height: 100vh; }
    h1, h2, h3 { color: #f8fafc; border-bottom: 2px solid transparent; border-image: linear-gradient(90deg, #3b82f6, #8b5cf6) 1; padding-bottom: 8px; margin-top: 0; }
    .card { background: rgba(30, 41, 59, 0.7); backdrop-filter: blur(10px); padding: 30px; border-radius: 16px; box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.5); margin-bottom: 30px; border: 1px solid rgba(255, 255, 255, 0.05); transition: transform 0.3s ease, box-shadow 0.3s ease; }
    .card:hover { transform: translateY(-5px); box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.5); }
    .alert { padding: 16px; border-radius: 8px; margin-bottom: 20px; font-weight: 500; }
    .success { background: rgba(16, 185, 129, 0.1); color: #34d399; border: 1px solid rgba(16, 185, 129, 0.3); }
    .warning { background: rgba(245, 158, 11, 0.1); color: #fbbf24; border: 1px solid rgba(245, 158, 11, 0.3); }
    a { color: #60a5fa; text-decoration: none; font-weight: 500; transition: all 0.2s ease; }
    a:hover { color: #93c5fd; text-shadow: 0 0 8px rgba(147, 197, 253, 0.5); }
    ul { list-style-type: none; padding-left: 0; }
    li { margin-bottom: 12px; background: rgba(15, 23, 42, 0.6); padding: 14px; border-radius: 8px; border: 1px solid rgba(255,255,255,0.05); border-left: 4px solid #3b82f6; transition: transform 0.2s; }
    li:hover { transform: translateX(5px); background: rgba(30, 41, 59, 0.8); }
    .btn { display: inline-block; padding: 10px 15px; background: linear-gradient(135deg, #3b82f6, #6366f1); color: white; font-weight: 600; border-radius: 8px; border: none; box-shadow: 0 4px 6px -1px rgba(59, 130, 246, 0.3); transition: all 0.3s ease; text-decoration: none; }
    .btn:hover { text-shadow: none; transform: translateY(-2px); box-shadow: 0 10px 15px -3px rgba(59, 130, 246, 0.5); color: white; }
    code { background: rgba(0, 0, 0, 0.3); padding: 4px 8px; border-radius: 6px; font-family: monospace; font-size: 0.9em; color: #a78bfa; }
</style>)";

User* get_current_user(const Request& req) {
    string user_id = get_session_cookie(req.headers);
    if (!user_id.empty() && users.count(user_id)) {
        return &users[user_id];
    }
    return nullptr;
}

void send_forbidden(Response& res, const string& message) {
    res.status = 403;
    res.set_content("<html><head><title>403 Forbidden</title>" + GLOBAL_CSS_STYLE + "</head><body><div class='card'><h1 style='color: #ef4444;'>403 Forbidden</h1><p style='color: #fca5a5; font-size: 1.1em;'>" + message + "</p><br><a href='/' class='btn'>Back to Home</a></div></body></html>", "text/html");
}

void send_redirect(Response& res, const string& url) {
    res.status = 302;
    res.set_header("Location", url.c_str());
}

bool require_role(const Request& req, Response& res, const vector<string>& allowed_roles, User** out_user) {
    User* user = get_current_user(req);
    *out_user = user;
    if (!user) {
        send_redirect(res, "/");
        return false;
    }
    bool allowed = false;
    for (const auto& r : allowed_roles) {
        if (user->role == r) { allowed = true; break; }
    }
    if (!allowed) {
        send_forbidden(res, "Access Denied: You must have proper roles for this area.");
        return false;
    }
    return true;
}

bool require_ownership_or_admin(const Request& req, Response& res, const string& target_user_id, User** out_user) {
    User* user = get_current_user(req);
    *out_user = user;
    if (!user) {
        send_redirect(res, "/");
        return false;
    }
    if (user->role == "Admin" || user->id == target_user_id) {
        return true;
    }
    send_forbidden(res, "Access Denied: You cannot view messages that do not belong to you.");
    return false;
}

void replace_all(string& str, const string& from, const string& to) {
    if(from.empty()) return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); 
    }
}

string read_html_template() {
    ifstream f("index.html");
    if (!f.is_open()) return "<h1>Error: index.html not found! Please run from the application folder.</h1>";
    stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}



int main(void) {
    Server svr;

    svr.Get("/", [&](const Request& req, Response& res) {
        User* current_user = get_current_user(req);
        string html = read_html_template();

        string user_info, admin_links, msg_links, demo_section;

        if (current_user) {
            user_info = "<div class='alert success'>Logged in as <strong>" + current_user->name + "</strong> (Role: " + current_user->role + ") | <a href='/logout'>Logout</a></div>";
            
            if (current_user->role == "Admin") {
                admin_links = "<li><a href='/vulnerable/admin'>Admin Dashboard (Vulnerable - Open to all)</a></li><li><a href='/secure/admin'>Admin Dashboard (Secure - RBAC protected)</a></li>";
            }
            msg_links = "<li><a href='/vulnerable/messages/" + current_user->id + "'>My Messages (Vulnerable)</a></li><li><a href='/secure/messages/" + current_user->id + "'>My Messages (Secure)</a></li>";

            demo_section = R"(
            <h3 style='color: white; margin-top:0;'>Test the Vulnerabilities!</h3>
            <p><strong>Vertical Escalation:</strong> If you are an Employee, you don't see the Admin link above. But what if you manually visit <a href='/vulnerable/admin'>/vulnerable/admin</a>?</p>
            <p><strong>Horizontal Escalation (IDOR):</strong> Try to read Charlie's messages by manually changing the ID in the URL to 3: <a href='/vulnerable/messages/3'>/vulnerable/messages/3</a></p>
            <p><em>Then, try the identical actions on the secure paths <code>/secure/...</code> to see RBAC in action!</em></p>
            )";
        } else {
            user_info = "<div class='alert warning'>Not logged in. Please select a user to simulate authentication:</div><ul>";
            for (const auto& pair : users) {
                user_info += "<li><a href='/login?user_id=" + pair.first + "'>Login as " + pair.second.name + " (" + pair.second.role + ")</a></li>";
            }
            user_info += "</ul>";
        }

        replace_all(html, "{{USER_INFO}}", user_info);
        replace_all(html, "{{ADMIN_LINKS}}", admin_links);
        replace_all(html, "{{MSG_LINKS}}", msg_links);
        replace_all(html, "{{DEMO_SECTION}}", demo_section);

        res.set_content(html, "text/html");
    });

    svr.Get("/login", [&](const Request& req, Response& res) {
        if (req.has_param("user_id")) {
            string user_id = req.get_param_value("user_id");
            if (users.count(user_id)) {
                res.status = 302;
                res.set_header("Set-Cookie", "session_user_id=" + user_id + "; Path=/");
                res.set_header("Location", "/");
                return;
            }
        }
        send_redirect(res, "/");
    });

    svr.Get("/logout", [&](const Request& /*req*/, Response& res) {
        res.status = 302;
        res.set_header("Set-Cookie", "session_user_id=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
        res.set_header("Location", "/");
    });

    // Phase 1: Vulnerable Routes
    svr.Get("/vulnerable/admin", [&](const Request& /*req*/, Response& res) {
        string html = "<html><head><title>Vulnerable Admin View</title>" + GLOBAL_CSS_STYLE + "</head><body><div class='card'><h1>Vulnerable Admin Dashboard</h1><p>Welcome to the top-secret admin dashboard!</p><p><strong>Status:</strong> Warning: No Server-Side Checks Enforced!</p><br><a href='/' class='btn'>Back to Home</a></div></body></html>";
        res.set_content(html, "text/html");
    });

    svr.Get(R"(^/vulnerable/messages/(\d+)$)", [&](const Request& req, Response& res) {
        string target_id = req.matches[1];
        if (!users.count(target_id)) {
            res.set_content("User not found.", "text/html");
            return;
        }
        string msgs;
        for (const auto& m : user_msgs[target_id]) msgs += "<li>" + m + "</li>";
        
        string html = "<html><head><title>Vulnerable Messages</title>" + GLOBAL_CSS_STYLE + "</head><body><div class='card'><h1>Vulnerable Messages for " + users[target_id].name + "</h1><ul>" + msgs + "</ul><br><a href='/' class='btn'>Back to Home</a></div></body></html>";
        res.set_content(html, "text/html");
    });

    // Phase 2: Secure Routes
    svr.Get("/secure/admin", [&](const Request& req, Response& res) {
        User* current_user = nullptr;
        if (!require_role(req, res, {"Admin"}, &current_user)) return;
        
        string html = "<html><head><title>Secure Admin View</title>" + GLOBAL_CSS_STYLE + "</head><body><div class='card'><h1>Secure Admin Dashboard</h1><p>Welcome to the top-secret admin dashboard!</p><p><strong>Status:</strong> Access verified via RBAC Middleware!</p><br><a href='/' class='btn'>Back to Home</a></div></body></html>";
        res.set_content(html, "text/html");
    });

    svr.Get(R"(^/secure/messages/(\d+)$)", [&](const Request& req, Response& res) {
        string target_id = req.matches[1];
        User* current_user = nullptr;
        if (!require_ownership_or_admin(req, res, target_id, &current_user)) return;
        
        if (!users.count(target_id)) {
            res.set_content("User not found.", "text/html");
            return;
        }
        string msgs;
        for (const auto& m : user_msgs[target_id]) msgs += "<li>" + m + "</li>";
        
        string html = "<html><head><title>Secure Messages</title>" + GLOBAL_CSS_STYLE + "</head><body><div class='card'><h1>Secure Messages for " + users[target_id].name + "</h1><ul>" + msgs + "</ul><br><a href='/' class='btn'>Back to Home</a></div></body></html>";
        res.set_content(html, "text/html");
    });

    cout << "Serving at http://localhost:8081" << endl;
    if (!svr.listen("0.0.0.0", 8081)) {
        cout << "Failed to start server." << endl;
        return 1;
    }
    return 0;
}
