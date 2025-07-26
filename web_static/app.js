// app.js

document.addEventListener('DOMContentLoaded', () => {
    
    // --- Application State ---
    const state = {
        isLoggedIn: !!localStorage.getItem('token'),
        username: localStorage.getItem('username') || '',
        token: localStorage.getItem('token') || ''
    };

    // --- WebSocket Manager ---
    const wsManager = {
        ws: null,
        connect() {
            if (this.ws) this.ws.close();
            const wsUrl = `wss://${window.location.host}/echo`;
            this.updateStatus('connecting', `正在连接到 ${wsUrl}...`);
            this.ws = new WebSocket(wsUrl);

            this.ws.onopen = () => {
                this.updateStatus('connected', '已连接');
                this.addMessage('成功连接到服务器!', 'received');
            };
            this.ws.onmessage = (event) => {
                this.addMessage(`${event.data}`, 'received');
            };
            this.ws.onclose = () => {
                this.updateStatus('disconnected', '连接已断开');
                this.ws = null;
            };
            this.ws.onerror = (error) => {
                this.updateStatus('disconnected', '连接错误');
                this.addMessage('连接发生错误，请查看浏览器控制台。', 'received');
                console.error('WebSocket Error:', error);
            };
        },
        disconnect() { if (this.ws) this.ws.close(); },
        sendMessage(message) {
            if (this.ws && this.ws.readyState === WebSocket.OPEN) {
                this.ws.send(message);
                this.addMessage(`${message}`, 'sent');
                return true;
            }
            return false;
        },
        updateStatus(status, text) {
            const UIElements = {
                statusEl: document.getElementById('ws-status'),
                inputEl: document.getElementById('ws-input'),
                sendBtn: document.getElementById('ws-send-btn')
            };
            if (UIElements.statusEl) {
                UIElements.statusEl.innerHTML = `<span class="status-dot ${status}"></span> ${text}`;
            }
            if(UIElements.inputEl && UIElements.sendBtn){
                const isConnected = status === 'connected';
                UIElements.inputEl.disabled = !isConnected;
                UIElements.sendBtn.disabled = !isConnected;
            }
        },
        addMessage(message, type) {
            const messagesEl = document.getElementById('ws-messages');
            if (messagesEl) {
                const msgDiv = document.createElement('div');
                msgDiv.className = type;
                msgDiv.textContent = message;
                messagesEl.appendChild(msgDiv);
                messagesEl.scrollTop = messagesEl.scrollHeight;
            }
        }
    };

    // --- API Service ---
    const api = {
        baseUrl: '/api',
        async post(endpoint, body) {
            try {
                const res = await fetch(this.baseUrl + endpoint, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(body)
                });
                const data = await res.json().catch(() => ({ message: `HTTP 错误: ${res.status}` }));
                return { ok: res.ok, data };
            } catch (err) {
                return { ok: false, data: { message: '网络错误，请稍后重试' } };
            }
        },
        login: (username, password) => api.post('/login', { username, password }),
        register: (username, password) => api.post('/register', { username, password })
    };

    // --- View Templates ---
    const views = {
        home: `
            <h1 class="page-title">项目主页</h1>
            <div class="card">
                <h2>C++ 高性能 WebServer</h2>
                <p>一个基于 C++17 实现的现代化、高性能 Web 服务器。它采用 Reactor 设计模式，结合 epoll (Linux) 实现高并发 I/O，并集成了多线程、SSL/TLS 加密、HTTP/1.1 解析、WebSocket 协议升级以及数据库连接池等核心功能。</p>
            </div>
        `,
        about: `
            <h1 class="page-title">关于项目</h1>
            <div class="card">
                <h2>核心技术栈</h2>
                <ul>
                    <li><strong>网络模型:</strong> 主从 Reactor 模式 (one loop per thread)</li>
                    <li><strong>I/O 复用:</strong> 基于 epoll 的事件驱动</li>
                    <li><strong>HTTP 处理:</strong> 自研 HTTP/1.1 解析器，支持路由与中间件</li>
                    <li><strong>实时通信:</strong> 实现 WebSocket 协议升级与数据帧解析</li>
                    <li><strong>数据库:</strong> 线程安全的 MySQL 连接池</li>
                    <li><strong>安全性:</strong> 集成 OpenSSL 实现 HTTPS，支持 JWT 用户认证</li>
                </ul>
            </div>
        `,
        login: `
            <div class="form-container">
                <div class="card">
                    <h2>账户登录</h2>
                    <form id="login-form">
                        <div id="form-message"></div>
                        <div class="form-group">
                            <label for="username">用户名</label>
                            <input type="text" id="username" class="form-control" required>
                        </div>
                        <div class="form-group">
                            <label for="password">密码</label>
                            <input type="password" id="password" class="form-control" required minlength="6">
                        </div>
                        <button type="submit" class="btn" style="width:100%;">登录</button>
                        <p class="form-text">没有账号？ <a href="#/register">立即注册</a></p>
                    </form>
                </div>
            </div>
        `,
        register: `
            <div class="form-container">
                 <div class="card">
                    <h2>创建新账户</h2>
                    <form id="register-form">
                        <div id="form-message"></div>
                        <div class="form-group">
                            <label for="username">用户名</label>
                            <input type="text" id="username" class="form-control" required>
                        </div>
                        <div class="form-group">
                            <label for="password">密码 (至少6位)</label>
                            <input type="password" id="password" class="form-control" required minlength="6">
                        </div>
                         <div class="form-group">
                            <label for="confirm-password">确认密码</label>
                            <input type="password" id="confirm-password" class="form-control" required minlength="6">
                        </div>
                        <button type="submit" class="btn" style="width:100%;">注册</button>
                        <p class="form-text">已有账号？ <a href="#/login">前往登录</a></p>
                    </form>
                </div>
            </div>
        `,
        websocket: `
            <h1 class="page-title">WebSocket 实时测试中心</h1>
            <div class="card">
                <div class="chat-container">
                    <div id="ws-status" class="chat-status"></div>
                    <div id="ws-messages" class="chat-messages"></div>
                    <form id="ws-form" class="chat-input-form">
                        <input id="ws-input" type="text" class="form-control" placeholder="输入消息..." autocomplete="off" disabled>
                        <button id="ws-send-btn" type="submit" class="btn" disabled>发送</button>
                    </form>
                </div>
            </div>
        `,
        notFound: `
            <div class="error-page-container">
                <svg class="graphic" viewBox="0 0 600 400" xmlns="http://www.w3.org/2000/svg">
                    <g transform="translate(300, 200)">
                        <path d="M150,0 A150,150 0 1,1 -150,0 A150,150 0 1,1 150,0" fill="none" stroke="${'#3a404d'}" stroke-width="8"/>
                        <text y="20" font-size="100" fill="${'#e2e2e2'}" text-anchor="middle" font-family="var(--font-family)" font-weight="bold">404</text>
                        <g transform="translate(-50, -80)">
                           <path d="M10,20 Q15,0 20,20" stroke="${'#a0a0a0'}" stroke-width="6" fill="none"/>
                        </g>
                         <g transform="translate(30, -80)">
                           <path d="M10,20 Q15,0 20,20" stroke="${'#a0a0a0'}" stroke-width="6" fill="none"/>
                        </g>
                        <path d="M-40,40 Q0,60 40,40" stroke="${'#a0a0a0'}" stroke-width="6" fill="none"/>
                    </g>
                </svg>
                <h2>页面迷失在代码的海洋中</h2>
                <p>我们似乎找不到您要寻找的页面。也许它被一个丢失的分号带走了，或者正在遥远的服务器上度假。</p>
                <a href="#/home" class="btn">返回安全的港湾</a>
            </div>
        `
    };

    // --- Router ---
    const router = () => {
        const path = window.location.hash.slice(1) || '/home';
        const routes = {
            '/home': views.home,
            '/about': views.about,
            '/websocket': state.isLoggedIn ? views.websocket : views.login,
            '/login': state.isLoggedIn ? views.home : views.login,
            '/register': state.isLoggedIn ? views.home : views.register
        };
        const mainContainer = document.getElementById('app-container');
        mainContainer.innerHTML = routes[path] || views.notFound;
        
        updateNavLinks(path);
        attachEventListeners(path);
    };

    // --- UI Update ---
    const updateNavLinks = (currentPath) => {
        document.querySelectorAll('.sidebar-nav a').forEach(link => {
            link.classList.toggle('active', link.dataset.route === currentPath);
        });
    };

    const updateAuthArea = () => {
        const authArea = document.getElementById('auth-area');
        if (state.isLoggedIn) {
            authArea.innerHTML = `
                <div class="auth-area-content">
                    <span class="username">${state.username}</span>
                    <button id="logout-btn" title="退出登录" class="btn">退出</button>
                </div>
            `;
            document.getElementById('logout-btn').addEventListener('click', handleLogout);
        } else {
            authArea.innerHTML = `<a href="#/login" class="btn">登录 / 注册</a>`;
        }
    };
    
    // --- Event Handlers ---
    const showMessage = (type, message) => {
        const msgEl = document.getElementById('form-message');
        if (msgEl) {
            msgEl.innerHTML = `<div class="form-message ${type}">${message}</div>`;
        }
    };

    const handleLogout = (e) => {
        e.preventDefault();
        localStorage.clear();
        Object.assign(state, { isLoggedIn: false, username: '', token: '' });
        updateAuthArea();
        navigateTo('/login');
    };

    const handleLogin = async (e) => {
        e.preventDefault();
        const form = e.target;
        const [username, password, btn] = [form.querySelector('#username').value, form.querySelector('#password').value, form.querySelector('button')];
        btn.disabled = true; btn.textContent = '登录中...';

        const { ok, data } = await api.login(username, password);
        if (ok) {
            Object.assign(state, { isLoggedIn: true, token: data.token, username });
            localStorage.setItem('token', data.token);
            localStorage.setItem('username', username);
            updateAuthArea();
            showMessage('success', '登录成功！正在跳转...');
            setTimeout(() => navigateTo('/home'), 1000);
        } else {
            showMessage('error', data.message || '登录失败');
            btn.disabled = false; btn.textContent = '登录';
        }
    };
    
    const handleRegister = async (e) => {
        e.preventDefault();
        const form = e.target;
        const [username, password, confirm] = [form.querySelector('#username').value, form.querySelector('#password').value, form.querySelector('#confirm-password').value];
        if (password !== confirm) return showMessage('error', '两次输入的密码不一致');
        const btn = form.querySelector('button');
        btn.disabled = true; btn.textContent = '注册中...';

        const { ok, data } = await api.register(username, password);
        if (ok) {
            showMessage('success', '注册成功！请登录。');
            setTimeout(() => navigateTo('/login'), 1500);
        } else {
            showMessage('error', data.message || '注册失败');
            btn.disabled = false; btn.textContent = '注册';
        }
    };

    const handleWsFormSubmit = (e) => {
        e.preventDefault();
        const input = document.getElementById('ws-input');
        if (input.value.trim() && wsManager.sendMessage(input.value.trim())) {
            input.value = '';
        }
    };

    const attachEventListeners = (path) => {
        const pathMap = {
            '/login': () => document.getElementById('login-form').addEventListener('submit', handleLogin),
            '/register': () => document.getElementById('register-form').addEventListener('submit', handleRegister),
            '/websocket': () => {
                wsManager.connect();
                document.getElementById('ws-form').addEventListener('submit', handleWsFormSubmit);
            }
        };
        (pathMap[path] || function(){})();
        if (path !== '/websocket') wsManager.disconnect();
    };
    
    const navigateTo = (path) => { window.location.hash = path; };

    // --- App Initialization ---
    const init = () => {
        window.addEventListener('hashchange', router);
        updateAuthArea();
        router();
    };

    init();
});