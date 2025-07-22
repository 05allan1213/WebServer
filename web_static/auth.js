// 登录状态管理脚本，所有页面通用
(function() {
    // 假设登录后 localStorage 里有 username
    function getUsername() {
        return localStorage.getItem('username');
    }
    function setUsername(name) {
        localStorage.setItem('username', name);
    }
    // 获取JWT token
    function getToken() {
        return localStorage.getItem('token');
    }
    // 封装fetch，自动带token
    window.jwtFetch = async function(url, options={}) {
        options.headers = options.headers || {};
        const token = getToken();
        if (token) {
            options.headers['Authorization'] = 'Bearer ' + token;
        }
        return fetch(url, options);
    }
    // 退出登录时清除token和username
    function clearAuth() {
        localStorage.removeItem('username');
        localStorage.removeItem('token');
    }
    function renderAuthArea() {
        var area = document.getElementById('authArea');
        if (!area) return;
        var username = getUsername();
        if (username) {
            area.innerHTML = '欢迎，' + username + ' | <a href="#" id="logoutLink" style="color:var(--accent-color);">退出登录</a>';
            var logout = document.getElementById('logoutLink');
            if (logout) {
                logout.onclick = function(e) {
                    e.preventDefault();
                    clearAuth();
                    window.location.href = '/login.html';
                };
            }
        } else {
            area.innerHTML = '<a href="/login.html">登录</a> / <a href="/register.html">注册</a>';
        }
    }
    // 登录页/注册页成功后写入用户名
    if (window.location.pathname === '/login.html') {
        var loginForm = document.getElementById('loginForm');
        if (loginForm) {
            loginForm.addEventListener('submit', function(e) {
                setTimeout(function() {
                    // 登录成功后写入用户名（假设后端返回用户名）
                    var msg = document.getElementById('loginMsg');
                    if (msg && msg.textContent.indexOf('登录成功') !== -1) {
                        var username = loginForm.username.value.trim();
                        setUsername(username);
                    }
                }, 1000);
            });
        }
    }
    if (window.location.pathname === '/register.html') {
        var regForm = document.getElementById('registerForm');
        if (regForm) {
            regForm.addEventListener('submit', function(e) {
                setTimeout(function() {
                    var msg = document.getElementById('registerMsg');
                    if (msg && msg.textContent.indexOf('注册成功') !== -1) {
                        var username = regForm.username.value.trim();
                        setUsername(username);
                    }
                }, 1000);
            });
        }
    }
    // 注册/登录页已登录自动跳转首页
    if ((window.location.pathname === '/login.html' || window.location.pathname === '/register.html') && getUsername()) {
        window.location.href = '/index.html';
    }
    // 受保护页面自动跳转登录
    if (window.location.pathname.startsWith('/protected') && !getToken()) {
        window.location.href = '/login.html';
    }
    // 渲染导航栏
    document.addEventListener('DOMContentLoaded', renderAuthArea);
})(); 