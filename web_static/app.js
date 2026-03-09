const state = {
    route: window.location.hash.replace("#", "") || "/dashboard",
    token: localStorage.getItem("token") || "",
    username: localStorage.getItem("username") || "",
    stats: null,
    statsError: "",
    apiResult: "",
    apiMeta: "",
    apiPreset: "ping",
    apiScenarioTitle: "Ping Baseline",
    apiScenarioEndpoint: "/perf/ping",
    apiScenarioDescription: "只压网络收发、路由命中和轻量 JSON 回包。",
    apiScenarioCommand: "wrk -t4 -c128 -d30s https://127.0.0.1:8443/perf/ping",
    profile: null,
    profileMessage: "",
    businessItems: [],
    businessMode: "memory",
    filePreview: "",
    fileMeta: "",
    activeFile: "manifest.json",
    ws: null,
    wsConnected: false,
    wsRoom: "lobby",
    wsNickname: localStorage.getItem("username") || `guest-${Math.random().toString(36).slice(2, 6)}`,
    wsDraft: "",
    wsMessages: [],
    wsStatus: "未连接",
    pollInterval: 3000
};

const apiPresets = {
    ping: {
        label: "GET /perf/ping",
        method: "GET",
        path: "/perf/ping",
        body: ""
    },
    json: {
        label: "GET /perf/json",
        method: "GET",
        path: "/perf/json",
        body: ""
    },
    echoJson: {
        label: "POST /perf/echo-json",
        method: "POST",
        path: "/perf/echo-json",
        body: JSON.stringify({ scene: "lab", intensity: "mixed", tags: ["parser", "body"] }, null, 2)
    },
    itemsMemory: {
        label: "GET /perf/items?mode=memory",
        method: "GET",
        path: "/perf/items?mode=memory&limit=8",
        body: ""
    },
    itemsMemory32: {
        label: "GET /perf/items?mode=memory&limit=32",
        method: "GET",
        path: "/perf/items?mode=memory&limit=32",
        body: ""
    },
    itemsDb: {
        label: "GET /perf/items?mode=db",
        method: "GET",
        path: "/perf/items?mode=db&limit=8",
        body: ""
    },
    itemsDb16: {
        label: "GET /perf/items?mode=db&limit=16",
        method: "GET",
        path: "/perf/items?mode=db&limit=16",
        body: ""
    },
    batchMemory: {
        label: "POST /perf/items/batch?mode=memory",
        method: "POST",
        path: "/perf/items/batch?mode=memory",
        body: JSON.stringify({
            items: [
                { name: "batch-alpha", category: "frontend", score: 88.2 },
                { name: "batch-beta", category: "network", score: 91.5 }
            ]
        }, null, 2)
    },
    batchDb: {
        label: "POST /perf/items/batch?mode=db",
        method: "POST",
        path: "/perf/items/batch?mode=db",
        body: JSON.stringify({
            items: [
                { name: "alpha", category: "db", score: 92.4 }
            ]
        }, null, 2)
    },
    fileTransfer: {
        label: "GET /perf/file/bundle.txt",
        method: "GET",
        path: "/perf/file/bundle.txt",
        body: ""
    },
    compute: {
        label: "GET /perf/compute",
        method: "GET",
        path: "/perf/compute?loops=90000",
        body: ""
    }
};

const scenarioCards = [
    {
        title: "Ping Baseline",
        presetKey: "ping",
        endpoint: "/perf/ping",
        description: "只压网络收发、路由命中和轻量 JSON 回包。",
        command: "wrk -t4 -c128 -d30s https://127.0.0.1:8443/perf/ping"
    },
    {
        title: "Mixed JSON",
        presetKey: "echoJson",
        endpoint: "/perf/echo-json",
        description: "解析请求体并做标准 JSON 回包，适合看 parser 与业务线程池。",
        command: "wrk -t4 -c96 -d30s -s scripts/post_echo.lua https://127.0.0.1:8443/perf/echo-json"
    },
    {
        title: "Memory Query",
        presetKey: "itemsMemory32",
        endpoint: "/perf/items?mode=memory&limit=32",
        description: "测内存仓储与中等响应体，不受外部数据库波动影响。",
        command: "wrk -t6 -c160 -d30s https://127.0.0.1:8443/perf/items?mode=memory&limit=32"
    },
    {
        title: "DB Query",
        presetKey: "itemsDb16",
        endpoint: "/perf/items?mode=db&limit=16",
        description: "连接池、SQL 与 API 聚合的真实后端读场景。",
        command: "wrk -t4 -c64 -d30s https://127.0.0.1:8443/perf/items?mode=db&limit=16"
    },
    {
        title: "Batch Write",
        presetKey: "batchDb",
        endpoint: "/perf/items/batch?mode=db",
        description: "批量写入与事务开销，适合观察数据库写路径。",
        command: "curl -k -X POST https://127.0.0.1:8443/perf/items/batch?mode=db -H 'Content-Type: application/json' -d '{\"items\":[{\"name\":\"alpha\",\"category\":\"db\",\"score\":92.4}]}'"
    },
    {
        title: "File Transfer",
        presetKey: "fileTransfer",
        endpoint: "/perf/file/bundle.txt",
        description: "走文件发送路径，观察大响应与下载耗时。",
        command: "wrk -t4 -c48 -d30s https://127.0.0.1:8443/perf/file/bundle.txt"
    }
];

const fileCards = [
    { name: "manifest.json", label: "Manifest", description: "场景说明与控制台元数据。" },
    { name: "bundle.txt", label: "Bundle Trace", description: "用于文件发送场景的示例大文本。" },
    { name: "dataset.csv", label: "Dataset CSV", description: "列表页和下载页共用的数据样本。" }
];

let pollTimer = null;
let renderScheduled = false;

function getProtocolAwareWsUrl() {
    const scheme = window.location.protocol === "https:" ? "wss" : "ws";
    return `${scheme}://${window.location.host}/ws/chat`;
}

function getAuthHeaders() {
    return state.token ? { Authorization: `Bearer ${state.token}` } : {};
}

function pageTitle(route) {
    const map = {
        "/dashboard": "仪表盘",
        "/api-lab": "接口实验室",
        "/scenarios": "压测场景",
        "/chat": "实时通信",
        "/business": "业务后台",
        "/files": "文件实验区"
    };
    return map[route] || "WebServer Control Deck";
}

function numberValue(value, fallback = 0) {
    return Number.isFinite(Number(value)) ? Number(value) : fallback;
}

function formatJson(value) {
    return JSON.stringify(value, null, 2);
}

function findScenarioByEndpoint(endpoint) {
    return scenarioCards.find((card) => card.endpoint === endpoint) || null;
}

function syncScenarioContextFromPreset() {
    const preset = apiPresets[state.apiPreset];
    const scenario = findScenarioByEndpoint(preset?.path);
    if (scenario) {
        state.apiScenarioTitle = scenario.title;
        state.apiScenarioEndpoint = scenario.endpoint;
        state.apiScenarioDescription = scenario.description;
        state.apiScenarioCommand = scenario.command;
        return;
    }
    state.apiScenarioTitle = "";
    state.apiScenarioEndpoint = preset?.path || "";
    state.apiScenarioDescription = "";
    state.apiScenarioCommand = "";
}

function resetApiLabResponse() {
    state.apiMeta = "";
    state.apiResult = "";
}

function escapeHtml(value) {
    return String(value)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#39;");
}

function safeChatBody(payload) {
    if (typeof payload?.text === "string" && payload.text.trim()) {
        return payload.text;
    }
    if (typeof payload?.message === "string" && payload.message.trim()) {
        return payload.message;
    }
    return formatJson(payload);
}

async function request(path, options = {}) {
    const startedAt = performance.now();
    const response = await fetch(path, options);
    const elapsed = (performance.now() - startedAt).toFixed(1);
    const contentType = response.headers.get("content-type") || "";
    let payload;

    if (contentType.includes("application/json")) {
        payload = await response.json();
    } else {
        payload = await response.text();
    }

    return { response, payload, elapsed };
}

async function refreshStats() {
    try {
        const { response, payload } = await request("/debug/perf-stats");
        if (!response.ok) {
            throw new Error(typeof payload === "string" ? payload : payload.message || `HTTP ${response.status}`);
        }
        state.stats = payload;
        state.statsError = "";
        state.pollInterval = Math.max(payload?.perf?.stats_poll_interval_ms || 3000, 3000);
    } catch (error) {
        state.statsError = error.message;
    }
    updateRailStatus();
    if (state.route === "/dashboard") {
        scheduleRender();
    }
}

function startPolling() {
    if (pollTimer) {
        clearTimeout(pollTimer);
    }
    const tick = async () => {
        if (!document.hidden) {
            await refreshStats();
        }
        pollTimer = setTimeout(tick, state.route === "/dashboard" ? state.pollInterval : 8000);
    };
    pollTimer = setTimeout(tick, state.pollInterval);
}

function scheduleRender() {
    if (renderScheduled) {
        return;
    }
    renderScheduled = true;
    requestAnimationFrame(() => {
        renderScheduled = false;
        render();
    });
}

function updateRailStatus() {
    const target = document.getElementById("rail-status");
    if (!target) {
        return;
    }

    const stats = state.stats;
    if (!stats) {
        target.innerHTML = `<div class="status-pill"><span>服务状态</span><strong>加载中</strong></div>`;
        return;
    }

    target.innerHTML = `
        <div class="status-pill"><span>请求总数</span><strong>${numberValue(stats.service.total_requests)}</strong></div>
        <div class="status-pill"><span>打开连接</span><strong>${numberValue(stats.service.open_connections)}</strong></div>
        <div class="status-pill"><span>WebSocket</span><strong>${numberValue(stats.service.websocket_clients)}</strong></div>
        <div class="status-pill"><span>DB 模式</span><strong>${stats.service.db_mode_available ? "ready" : "offline"}</strong></div>
    `;
}

function routeRows() {
    const routes = state.stats?.routes || [];
    if (!routes.length) {
        return `<div class="empty">还没有采样数据，先从接口实验室或浏览器访问几个接口。</div>`;
    }
    return `
        <table class="route-table">
            <thead>
                <tr>
                    <th>Route</th>
                    <th>Requests</th>
                    <th>Errors</th>
                    <th>Avg us</th>
                    <th>P95 us</th>
                </tr>
            </thead>
            <tbody>
                ${routes.slice(0, 8).map((route) => `
                    <tr>
                        <td>${route.path}</td>
                        <td>${numberValue(route.requests)}</td>
                        <td>${numberValue(route.errors)}</td>
                        <td>${numberValue(route.avg_us)}</td>
                        <td>${numberValue(route.p95_us)}</td>
                    </tr>
                `).join("")}
            </tbody>
        </table>
    `;
}

function dashboardView() {
    const stats = state.stats;
    if (!stats) {
        return `<div class="hero"><div class="empty">正在拉取运行时快照...</div></div>`;
    }

    const totalRequests = numberValue(stats.service.total_requests);
    const uptime = Math.max(numberValue(stats.service.uptime_seconds), 1);
    const qps = (totalRequests / uptime).toFixed(2);
    const dbStatus = stats.service.db_mode_available ? "数据库模式已就绪" : "数据库模式离线，memory 仍可演示";
    const routeLead = stats.routes?.[0];
    const routeHeat = routeLead ? Math.min(100, Math.max(8, (numberValue(routeLead.requests) / Math.max(totalRequests, 1)) * 100)) : 12;

    return `
        <section class="hero card">
            <div class="hero-grid">
                <div>
                    <p class="eyebrow">Runtime Overview</p>
                    <h3>把 C++ 网络框架的 IO、业务线程池、文件发送与 WebSocket 放在同一个操控台里看。</h3>
                    <p>${dbStatus}</p>
                    <div class="tag-row">
                        <span class="tag">memory dataset ${numberValue(stats.service.memory_dataset_size)}</span>
                        <span class="tag signal">ws clients ${numberValue(stats.service.websocket_clients)}</span>
                        <span class="tag">open connections ${numberValue(stats.service.open_connections)}</span>
                    </div>
                </div>
                <div class="card">
                    <p class="eyebrow">Hot Route</p>
                    <h3>${routeLead ? routeLead.path : "/perf/ping"}</h3>
                    <p>最近最活跃的接口会在这里浮起来，方便直接切回接口实验室继续看。</p>
                    <div class="line-chart"><span style="width:${routeHeat}%"></span></div>
                </div>
            </div>
        </section>

        <section class="metric-grid">
            <article class="metric-card">
                <p class="eyebrow">QPS Approx</p>
                <span class="metric-value metric-accent">${qps}</span>
                <p>按 uptime 和累计请求近似计算</p>
            </article>
            <article class="metric-card">
                <p class="eyebrow">Errors</p>
                <span class="metric-value">${numberValue(stats.service.total_errors)}</span>
                <p>全局 4xx/5xx 统计</p>
            </article>
            <article class="metric-card">
                <p class="eyebrow">Broadcast Fanout</p>
                <span class="metric-value metric-signal">${numberValue(stats.service.websocket_broadcast_fanout)}</span>
                <p>聊天室广播的累计扇出数</p>
            </article>
            <article class="metric-card">
                <p class="eyebrow">Buffer Resize</p>
                <span class="metric-value">${numberValue(stats.buffer.resize_count)}</span>
                <p>底层 Buffer 扩容次数</p>
            </article>
        </section>

        <section class="dual-grid">
            <article class="card">
                <h3 class="section-title">核心服务指标</h3>
                <div class="list-stack">
                    <div class="list-row"><span>运行时长</span><strong>${uptime}s</strong></div>
                    <div class="list-row"><span>累计请求</span><strong>${totalRequests}</strong></div>
                    <div class="list-row"><span>活跃 TCP 连接</span><strong>${numberValue(stats.service.open_connections)}</strong></div>
                    <div class="list-row"><span>在线 WebSocket</span><strong>${numberValue(stats.service.websocket_clients)}</strong></div>
                </div>
            </article>
            <article class="card">
                <h3 class="section-title">Buffer 观测</h3>
                <div class="list-stack">
                    <div class="list-row"><span>active buffers</span><strong>${numberValue(stats.buffer.active_count)}</strong></div>
                    <div class="list-row"><span>pool memory</span><strong>${numberValue(stats.buffer.pool_memory_bytes)} B</strong></div>
                    <div class="list-row"><span>heap memory</span><strong>${numberValue(stats.buffer.heap_memory_bytes)} B</strong></div>
                    <div class="list-row"><span>poll interval</span><strong>${numberValue(stats.perf.stats_poll_interval_ms)} ms</strong></div>
                </div>
            </article>
        </section>

        <section class="card">
            <h3 class="section-title">路由热度面板</h3>
            ${routeRows()}
        </section>
    `;
}

function apiLabView() {
    const preset = apiPresets[state.apiPreset];
    return `
        <section class="dual-grid">
            <article class="card">
                <h3 class="section-title">接口实验室</h3>
                <div class="field">
                    <label for="preset-select">预设场景</label>
                    <select id="preset-select">
                        ${Object.entries(apiPresets).map(([key, value]) => `
                            <option value="${key}" ${state.apiPreset === key ? "selected" : ""}>${value.label}</option>
                        `).join("")}
                    </select>
                </div>
                <div class="field-grid">
                    <div class="field">
                        <label for="api-method">Method</label>
                        <select id="api-method">
                            ${["GET", "POST", "HEAD"].map((method) => `
                                <option value="${method}" ${preset.method === method ? "selected" : ""}>${method}</option>
                            `).join("")}
                        </select>
                    </div>
                    <div class="field">
                        <label for="api-path">Path</label>
                        <input id="api-path" value="${preset.path}">
                    </div>
                </div>
                <div class="field">
                    <label for="api-body">Body</label>
                    <textarea id="api-body">${preset.body}</textarea>
                </div>
                <div class="button-row">
                    <button id="send-api-btn" class="solid-btn">发送请求</button>
                    <button id="load-preset-btn" class="ghost-btn">重载预设</button>
                </div>
            </article>
            <article class="api-result">
                <h3 class="section-title">响应面板</h3>
                <p class="eyebrow">${state.apiMeta || "等待请求"}</p>
                <pre>${state.apiResult || "运行结果会显示在这里。"}</pre>
            </article>
        </section>
        <section class="card" style="margin-top:18px;">
            <div class="button-row" style="justify-content:space-between; align-items:center;">
                <div>
                    <p class="eyebrow">${escapeHtml(state.apiScenarioEndpoint || "Scenario Command")}</p>
                    <h3 class="section-title" style="margin-bottom:6px;">${escapeHtml(state.apiScenarioTitle || "当前接口暂无绑定压测命令")}</h3>
                    <p style="margin:0; color:var(--muted);">${escapeHtml(state.apiScenarioDescription || "从“压测场景”带入后，这里会同步显示该场景的用途说明。")}</p>
                </div>
                <button id="copy-lab-command-btn" class="pill-btn" ${state.apiScenarioCommand ? "" : "disabled"}>复制命令</button>
            </div>
            <div class="code-block" style="margin-top:12px;">
                <pre>${escapeHtml(state.apiScenarioCommand || "从“压测场景”带入后，这里会同步显示对应 wrk/curl 命令。")}</pre>
            </div>
        </section>
    `;
}

function scenariosView() {
    return `
        <section class="scenario-grid">
            ${scenarioCards.map((card) => `
                <article class="card">
                    <p class="eyebrow">${card.endpoint}</p>
                    <h3>${card.title}</h3>
                    <p>${card.description}</p>
                    <div class="code-block"><pre>${card.command}</pre></div>
                    <div class="button-row">
                        <button class="pill-btn use-scenario-btn" data-endpoint="${card.endpoint}">带入实验室</button>
                        <button class="pill-btn copy-command-btn" data-command="${card.command.replace(/"/g, "&quot;")}">复制命令</button>
                    </div>
                </article>
            `).join("")}
        </section>
    `;
}

function chatView() {
    return `
        <section class="dual-grid">
            <article class="card chat-console">
                <h3 class="section-title">聊天室控制</h3>
                <div class="notice">
                    实时通信不要求登录。未登录时会以匿名访客身份进入房间。
                </div>
                <div class="chat-meta-row">
                    <div class="tag">房间: ${state.wsRoom}</div>
                    <div class="tag signal">${state.wsConnected ? "实时广播已联通" : "等待建立连接"}</div>
                </div>
                <div class="field-grid">
                    <div class="field">
                        <label for="ws-room-input">房间</label>
                        <input id="ws-room-input" value="${escapeHtml(state.wsRoom)}">
                    </div>
                    <div class="field">
                        <label for="ws-name-input">昵称</label>
                        <input id="ws-name-input" value="${escapeHtml(state.wsNickname)}">
                    </div>
                </div>
                <div class="button-row">
                    <button id="ws-connect-btn" class="solid-btn">${state.wsConnected ? "重新加入" : "连接聊天室"}</button>
                    <button id="ws-disconnect-btn" class="ghost-btn">断开</button>
                </div>
                <div class="notice ${state.wsConnected ? "ok" : ""}">
                    ${state.wsStatus}
                </div>
                <div class="field compose-field">
                    <label for="ws-message-input">消息</label>
                    <textarea id="ws-message-input" placeholder="直接输入消息内容，按 Ctrl+Enter 可快速发送。">${escapeHtml(state.wsDraft)}</textarea>
                </div>
                <div class="compose-hint">默认按普通文本发送；如果粘贴 JSON 对象，也会按协议消息发出。</div>
                <button id="ws-send-btn" class="solid-btn">发送消息</button>
            </article>
            <article class="chat-feed" id="chat-feed">
                ${state.wsMessages.length ? state.wsMessages.map(renderChatBubble).join("") : `<div class="empty">连接后这里会实时出现广播和系统消息。</div>`}
            </article>
        </section>
    `;
}

function renderChatBubble(item) {
    return `
        <div class="chat-bubble ${item.type === "presence" || item.type === "welcome" ? "system" : ""}">
            <strong>${escapeHtml(item.title)}</strong>
            <div>${escapeHtml(item.body)}</div>
        </div>
    `;
}

function businessView() {
    return `
        <section class="business-grid">
            <article class="auth-card">
                <h3>账户与认证</h3>
                <div class="auth-panels">
                    <form id="login-form" class="field">
                        <label>登录用户名</label>
                        <input id="login-username" value="${escapeHtml(state.username || "demo-user")}">
                        <label>密码</label>
                        <input id="login-password" type="password" placeholder="请输入密码" autocomplete="current-password">
                        <button class="solid-btn" type="submit">登录</button>
                    </form>
                    <form id="register-form" class="field">
                        <label>注册用户名</label>
                        <input id="register-username" value="${escapeHtml(`perf-user-${Date.now().toString().slice(-4)}`)}">
                        <label>密码</label>
                        <input id="register-password" type="password" placeholder="请输入密码" autocomplete="new-password">
                        <button class="ghost-btn" type="submit">注册</button>
                    </form>
                </div>
                <div id="auth-message" class="notice">${state.profileMessage || "登录后可读取 /api/profile，并携带 Bearer Token 访问。"} </div>
            </article>
            <article class="auth-card">
                <h3>个人资料与业务数据</h3>
                <div class="button-row">
                    <button id="load-profile-btn" class="pill-btn">读取 /api/profile</button>
                    <button id="load-items-memory-btn" class="pill-btn">加载 memory items</button>
                    <button id="load-items-db-btn" class="pill-btn">加载 db items</button>
                    <button id="insert-items-btn" class="solid-btn">批量写入当前模式</button>
                </div>
                <div class="code-block"><pre>${state.profile ? formatJson(state.profile) : "尚未读取资料或业务数据。"}</pre></div>
            </article>
        </section>
    `;
}

function filesView() {
    return `
        <section class="file-grid">
            ${fileCards.map((card) => `
                <article class="card">
                    <p class="eyebrow">${card.name}</p>
                    <h3>${card.label}</h3>
                    <p>${card.description}</p>
                    <div class="button-row">
                        <button class="pill-btn file-head-btn" data-file="${card.name}">查看元数据</button>
                        <button class="pill-btn file-preview-btn" data-file="${card.name}">预览内容</button>
                        <a class="solid-btn" href="/perf/file/${card.name}" target="_blank" rel="noreferrer">直接下载</a>
                    </div>
                </article>
            `).join("")}
        </section>
        <section class="dual-grid" style="margin-top:18px;">
            <article class="api-result">
                <h3 class="section-title">文件响应头</h3>
                <pre>${state.fileMeta || "选择一个文件查看 HEAD/GET 元信息。"}</pre>
            </article>
            <article class="api-result">
                <h3 class="section-title">文件预览</h3>
                <pre>${state.filePreview || "这里会展示前 2KB 内容。"}</pre>
            </article>
        </section>
    `;
}

function render() {
    document.getElementById("page-title").textContent = pageTitle(state.route);
    document.querySelectorAll(".nav-stack a").forEach((link) => {
        link.classList.toggle("active", link.dataset.route === state.route);
    });
    document.getElementById("auth-action-btn").textContent = state.token ? `已登录: ${state.username || "user"}` : "登录";

    const root = document.getElementById("app-root");
    const views = {
        "/dashboard": dashboardView,
        "/api-lab": apiLabView,
        "/scenarios": scenariosView,
        "/chat": chatView,
        "/business": businessView,
        "/files": filesView
    };

    root.innerHTML = (views[state.route] || dashboardView)();
    bindPageEvents();
}

function bindPageEvents() {
    const refreshAllBtn = document.getElementById("refresh-all-btn");
    if (refreshAllBtn) {
        refreshAllBtn.onclick = async () => {
            await refreshStats();
            scheduleRender();
        };
    }

    const authActionBtn = document.getElementById("auth-action-btn");
    if (authActionBtn) {
        authActionBtn.onclick = () => {
            if (state.token) {
                logout();
                return;
            }
            window.location.hash = "#/business";
        };
    }

    const presetSelect = document.getElementById("preset-select");
    if (presetSelect) {
        presetSelect.onchange = (event) => {
            state.apiPreset = event.target.value;
            syncScenarioContextFromPreset();
            resetApiLabResponse();
            scheduleRender();
        };
    }

    const loadPresetBtn = document.getElementById("load-preset-btn");
    if (loadPresetBtn) {
        loadPresetBtn.onclick = () => {
            syncScenarioContextFromPreset();
            resetApiLabResponse();
            scheduleRender();
        };
    }

    const sendApiBtn = document.getElementById("send-api-btn");
    if (sendApiBtn) {
        sendApiBtn.onclick = sendApiRequest;
    }

    document.querySelectorAll(".use-scenario-btn").forEach((button) => {
        button.onclick = () => {
            const endpoint = button.dataset.endpoint || "/perf/ping";
            const scenario = findScenarioByEndpoint(endpoint);
            state.apiPreset = scenario?.presetKey || Object.keys(apiPresets).find((key) => apiPresets[key].path === endpoint) || "ping";
            state.apiScenarioTitle = scenario?.title || "";
            state.apiScenarioEndpoint = scenario?.endpoint || endpoint;
            state.apiScenarioDescription = scenario?.description || "";
            state.apiScenarioCommand = scenario?.command || "";
            resetApiLabResponse();
            window.location.hash = "#/api-lab";
        };
    });

    document.querySelectorAll(".copy-command-btn").forEach((button) => {
        button.onclick = async () => {
            await navigator.clipboard.writeText(button.dataset.command || "");
            button.textContent = "已复制";
            setTimeout(() => {
                button.textContent = "复制命令";
            }, 1200);
        };
    });

    const copyLabCommandBtn = document.getElementById("copy-lab-command-btn");
    if (copyLabCommandBtn) {
        copyLabCommandBtn.onclick = async () => {
            if (!state.apiScenarioCommand) {
                return;
            }
            await navigator.clipboard.writeText(state.apiScenarioCommand);
            copyLabCommandBtn.textContent = "已复制";
            setTimeout(() => {
                copyLabCommandBtn.textContent = "复制命令";
            }, 1200);
        };
    }

    const wsConnectBtn = document.getElementById("ws-connect-btn");
    if (wsConnectBtn) {
        wsConnectBtn.onclick = connectChat;
    }

    const wsDisconnectBtn = document.getElementById("ws-disconnect-btn");
    if (wsDisconnectBtn) {
        wsDisconnectBtn.onclick = disconnectChat;
    }

    const wsSendBtn = document.getElementById("ws-send-btn");
    if (wsSendBtn) {
        wsSendBtn.onclick = sendChatMessage;
    }

    const wsMessageInput = document.getElementById("ws-message-input");
    if (wsMessageInput) {
        wsMessageInput.oninput = (event) => {
            state.wsDraft = event.target.value;
        };
        wsMessageInput.onkeydown = (event) => {
            if ((event.ctrlKey || event.metaKey) && event.key === "Enter") {
                event.preventDefault();
                sendChatMessage();
            }
        };
    }

    const loginForm = document.getElementById("login-form");
    if (loginForm) {
        loginForm.onsubmit = login;
    }

    const registerForm = document.getElementById("register-form");
    if (registerForm) {
        registerForm.onsubmit = registerUser;
    }

    const loadProfileBtn = document.getElementById("load-profile-btn");
    if (loadProfileBtn) {
        loadProfileBtn.onclick = loadProfile;
    }

    const loadItemsMemoryBtn = document.getElementById("load-items-memory-btn");
    if (loadItemsMemoryBtn) {
        loadItemsMemoryBtn.onclick = () => loadItems("memory");
    }

    const loadItemsDbBtn = document.getElementById("load-items-db-btn");
    if (loadItemsDbBtn) {
        loadItemsDbBtn.onclick = () => loadItems("db");
    }

    const insertItemsBtn = document.getElementById("insert-items-btn");
    if (insertItemsBtn) {
        insertItemsBtn.onclick = insertItems;
    }

    document.querySelectorAll(".file-head-btn").forEach((button) => {
        button.onclick = () => inspectFile(button.dataset.file, true);
    });

    document.querySelectorAll(".file-preview-btn").forEach((button) => {
        button.onclick = () => inspectFile(button.dataset.file, false);
    });
}

async function sendApiRequest() {
    const method = document.getElementById("api-method").value;
    const path = document.getElementById("api-path").value;
    const body = document.getElementById("api-body").value;

    const options = {
        method,
        headers: {
            ...getAuthHeaders()
        }
    };

    if (method !== "GET" && method !== "HEAD") {
        options.headers["Content-Type"] = "application/json";
        options.body = body;
    }

    try {
        const { response, payload, elapsed } = await request(path, options);
        state.apiMeta = `${method} ${path} -> ${response.status} in ${elapsed} ms`;
        state.apiResult = typeof payload === "string" ? payload : formatJson(payload);
    } catch (error) {
        state.apiMeta = `${method} ${path} -> failed`;
        state.apiResult = error.message;
    }
    scheduleRender();
}

function pushChatMessage(title, body, type = "message") {
    state.wsMessages.unshift({ title, body, type });
    state.wsMessages = state.wsMessages.slice(0, 30);
    if (state.route === "/chat") {
        const feed = document.getElementById("chat-feed");
        if (feed) {
            feed.innerHTML = state.wsMessages.map(renderChatBubble).join("");
        } else {
            scheduleRender();
        }
    }
}

function connectChat() {
    disconnectChat();

    const room = (document.getElementById("ws-room-input")?.value || "lobby").trim() || "lobby";
    const nickname = (document.getElementById("ws-name-input")?.value || `guest-${Math.random().toString(36).slice(2, 6)}`).trim() || `guest-${Math.random().toString(36).slice(2, 6)}`;
    state.wsRoom = room;
    state.wsNickname = nickname;

    const ws = new WebSocket(getProtocolAwareWsUrl());
    state.ws = ws;
    state.wsStatus = `连接中: ${ws.url}`;
    scheduleRender();

    ws.onopen = () => {
        state.wsConnected = true;
        state.wsStatus = `已连接到 ${room}`;
        ws.send(JSON.stringify({ type: "join", room, nickname }));
        pushChatMessage("system", `已连接，准备加入 ${room}`, "welcome");
        scheduleRender();
    };

    ws.onmessage = (event) => {
        try {
            const payload = JSON.parse(event.data);
            const title = payload.type === "message"
                ? `${payload.sender || "system"} @ ${payload.room || state.wsRoom}`
                : payload.type;
            const body = safeChatBody(payload);
            pushChatMessage(title, body, payload.type);
        } catch (_) {
            pushChatMessage("raw", event.data, "message");
        }
    };

    ws.onclose = (event) => {
        state.wsConnected = false;
        state.wsStatus = event.code ? `连接已关闭 (${event.code}${event.reason ? `: ${event.reason}` : ""})` : "连接已关闭";
        state.ws = null;
        scheduleRender();
    };

    ws.onerror = () => {
        state.wsStatus = "WebSocket 连接异常";
        scheduleRender();
    };
}

function disconnectChat() {
    if (state.ws) {
        state.ws.close();
        state.ws = null;
    }
    state.wsConnected = false;
    state.wsStatus = "未连接";
    if (state.route === "/chat") {
        scheduleRender();
    }
}

function sendChatMessage() {
    if (!state.ws || state.ws.readyState !== WebSocket.OPEN) {
        pushChatMessage("system", "请先建立 WebSocket 连接", "presence");
        return;
    }
    const raw = (document.getElementById("ws-message-input")?.value ?? state.wsDraft).trim();
    if (!raw) {
        pushChatMessage("system", "消息内容不能为空", "presence");
        return;
    }

    let outgoing = { type: "message", text: raw };
    if (raw.startsWith("{")) {
        try {
            const parsed = JSON.parse(raw);
            if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) {
                outgoing = parsed;
            }
        } catch (_) {
        }
    }

    if (typeof outgoing.type !== "string" || !outgoing.type.trim()) {
        outgoing.type = "message";
    }
    if (outgoing.type === "join") {
        outgoing.room = typeof outgoing.room === "string" && outgoing.room.trim() ? outgoing.room : state.wsRoom;
        outgoing.nickname = typeof outgoing.nickname === "string" && outgoing.nickname.trim() ? outgoing.nickname : state.wsNickname;
    } else if (typeof outgoing.text !== "string" || !outgoing.text.trim()) {
        outgoing.text = raw;
    }

    state.ws.send(JSON.stringify(outgoing));
    state.wsDraft = "";
    const input = document.getElementById("ws-message-input");
    if (input) {
        input.value = "";
    }
}

async function login(event) {
    event.preventDefault();
    const username = document.getElementById("login-username").value;
    const password = document.getElementById("login-password").value;

    if (!password) {
        state.profileMessage = "请输入密码";
        scheduleRender();
        return;
    }

    try {
        const { response, payload } = await request("/api/login", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ username, password })
        });
        if (!response.ok) {
            throw new Error(payload.message || `HTTP ${response.status}`);
        }
        state.token = payload.token;
        state.username = username;
        state.wsNickname = username;
        state.profileMessage = "登录成功";
        localStorage.setItem("token", state.token);
        localStorage.setItem("username", username);
    } catch (error) {
        state.profileMessage = `登录失败: ${error.message}`;
    }
    scheduleRender();
}

function logout() {
    state.token = "";
    state.username = "";
    state.profile = null;
    state.profileMessage = "已退出登录";
    state.wsNickname = `guest-${Math.random().toString(36).slice(2, 6)}`;
    localStorage.removeItem("token");
    localStorage.removeItem("username");
    disconnectChat();
    scheduleRender();
}

async function registerUser(event) {
    event.preventDefault();
    const username = document.getElementById("register-username").value;
    const password = document.getElementById("register-password").value;

    if (!password) {
        state.profileMessage = "请输入密码";
        scheduleRender();
        return;
    }

    try {
        const { response, payload } = await request("/api/register", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ username, password })
        });
        if (!response.ok) {
            throw new Error(payload.message || `HTTP ${response.status}`);
        }
        state.profileMessage = "注册成功，可以直接登录";
    } catch (error) {
        state.profileMessage = `注册失败: ${error.message}`;
    }
    scheduleRender();
}

async function loadProfile() {
    try {
        const { response, payload } = await request("/api/profile", {
            headers: getAuthHeaders()
        });
        if (!response.ok) {
            throw new Error(payload.error || payload.message || `HTTP ${response.status}`);
        }
        state.profile = payload;
        state.profileMessage = "资料读取成功";
    } catch (error) {
        state.profile = null;
        state.profileMessage = `资料读取失败: ${error.message}`;
    }
    scheduleRender();
}

async function loadItems(mode) {
    state.businessMode = mode;
    try {
        const { response, payload } = await request(`/perf/items?mode=${mode}&limit=8`);
        if (!response.ok) {
            throw new Error(payload.message || `HTTP ${response.status}`);
        }
        state.businessItems = payload.items || [];
        state.profile = payload;
        state.profileMessage = `已加载 ${mode} items`;
    } catch (error) {
        state.profileMessage = `加载 items 失败: ${error.message}`;
    }
    scheduleRender();
}

async function insertItems() {
    const mode = state.businessMode || "memory";
    try {
        const { response, payload } = await request(`/perf/items/batch?mode=${mode}`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
                items: [
                    { name: `${mode}-console-${Date.now()}`, category: "console", score: 86.4 },
                    { name: `${mode}-runtime-${Date.now() + 1}`, category: "runtime", score: 91.2 }
                ]
            })
        });
        if (!response.ok) {
            throw new Error(payload.message || `HTTP ${response.status}`);
        }
        state.profile = payload;
        state.profileMessage = `已向 ${mode} 模式写入 ${payload.inserted} 条数据`;
    } catch (error) {
        state.profileMessage = `写入失败: ${error.message}`;
    }
    scheduleRender();
}

async function inspectFile(fileName, headOnly) {
    state.activeFile = fileName;
    try {
        if (headOnly) {
            const response = await fetch(`/perf/file/${fileName}`, { method: "HEAD" });
            const headers = {};
            response.headers.forEach((value, key) => {
                headers[key] = value;
            });
            state.fileMeta = formatJson({ status: response.status, headers });
        } else {
            const { response, payload, elapsed } = await request(`/perf/file/${fileName}`);
            state.fileMeta = formatJson({
                status: response.status,
                elapsed_ms: elapsed,
                content_type: response.headers.get("content-type"),
                content_length: response.headers.get("content-length")
            });
            state.filePreview = typeof payload === "string" ? payload.slice(0, 2048) : formatJson(payload);
        }
    } catch (error) {
        state.fileMeta = `文件请求失败: ${error.message}`;
    }
    scheduleRender();
}

window.addEventListener("hashchange", () => {
    state.route = window.location.hash.replace("#", "") || "/dashboard";
    scheduleRender();
});

document.addEventListener("DOMContentLoaded", async () => {
    await refreshStats();
    startPolling();
    render();
});
