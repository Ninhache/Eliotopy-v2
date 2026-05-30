let currentConfig = { gridOverlay: true };
let initialized = false;

const el = id => document.getElementById(id);
const text = (id, val) => { if (el(id)) el(id).innerText = val; };
const show = (id, visible) => { if (el(id)) el(id).style.display = visible ? 'flex' : 'none'; };
const showBlock = (id, visible) => { if (el(id)) el(id).style.display = visible ? 'block' : 'none'; };

function applyUi(ui) {
    document.body.classList.toggle('collapsed', !!ui.collapsed);
    ['top', 'bottom', 'left', 'right'].forEach(d =>
        document.body.classList.toggle('dock-' + d, ui.dock === d));
}

let lastKeybindsSignature = '';

function post(msg) {
    if (window.chrome && window.chrome.webview) window.chrome.webview.postMessage(msg);
}

function onKeybindClick(id, capturing) {
    post(capturing ? 'kbcancel' : 'kbcapture:' + id);
}

function clearKeybind(id, event) {
    event.stopPropagation();
    post('kbclear:' + id);
}

function renderKeybinds(list, capturingId) {
    const signature = JSON.stringify(list) + '|' + (capturingId || '');
    if (signature === lastKeybindsSignature) return;
    lastKeybindsSignature = signature;

    for (const a of list) {
        const host = el('kb-ctrl-' + a.id);
        if (!host) continue;
        const capturing = a.id === capturingId;
        const chipText = capturing ? t('kb.press') : (a.bind || t('kb.unbound'));
        const cls = 'keybind-chip' + (capturing ? ' capturing' : '') + (!a.bind ? ' unbound' : '');
        host.innerHTML = `<span class="${cls}" onclick="onKeybindClick('${a.id}', ${capturing})">${chipText}</span>
            <span class="kb-clear" onclick="clearKeybind('${a.id}', event)" title="${t('kb.clear')}">
                <svg viewBox="0 0 24 24"><path d="M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"/></svg>
            </span>`;
    }
}

document.querySelectorAll('.tab').forEach(tab => {
    tab.addEventListener('click', () => {
        document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
        tab.classList.add('active');
        el('view-' + tab.dataset.target).classList.add('active');
    });
});

document.querySelectorAll('#logger-segment .segment').forEach(seg => {
    seg.addEventListener('click', () => {
        document.querySelectorAll('#logger-segment .segment').forEach(s => s.classList.remove('active'));
        seg.classList.add('active');
        setLogLevel(seg.dataset.val);
    });
});

document.querySelectorAll('#grid-mask-segment .segment').forEach(seg => {
    seg.addEventListener('click', () => {
        if (seg.dataset.type === 'mode') {
            document.querySelectorAll('#grid-mask-segment .segment[data-type="mode"]').forEach(s => s.classList.remove('active'));
            seg.classList.add('active');
            toggleConfig('gridMode', seg.dataset.val);
        } else if (seg.dataset.type === 'mask') {
            seg.classList.toggle('active');
            if (seg.dataset.val === 'los') toggleConfig('gridShowLos', seg.classList.contains('active'));
            if (seg.dataset.val === 'move') toggleConfig('gridShowMove', seg.classList.contains('active'));
        }
    });
});

document.querySelectorAll('#portal-shape-segment .segment').forEach(seg => {
    seg.addEventListener('click', () => {
        document.querySelectorAll('#portal-shape-segment .segment').forEach(s => s.classList.remove('active'));
        seg.classList.add('active');
        toggleConfig('portalNumberShape', seg.dataset.val);
    });
});

let appliedLang = null;

document.querySelectorAll('#lang-segment .segment').forEach(seg => {
    seg.addEventListener('click', () => {
        document.querySelectorAll('#lang-segment .segment').forEach(s => s.classList.remove('active'));
        seg.classList.add('active');
        appliedLang = seg.dataset.val;
        toggleConfig('language', seg.dataset.val);
        applyI18n(seg.dataset.val);
        lastKeybindsSignature = '';
    });
});

function toggleConfig(key, value) {
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage(`config:{"${key}":${JSON.stringify(value)}}`);
    }
}

function toggleConsole(show) {
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage(`console:${show}`);
    }
}

function setLogLevel(level) {
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage(`loglevel:${level}`);
    }
}

function setPollRate(rate) {
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage(`poll:${rate}`);
    }
}

function updateDebugView(state) {
    if (state.player) {
        showBlock('card-player', true);
        let trackClass = (state.player.id === state.trackedEntityId) ? 'border: 1px solid #ef4444; background: rgba(239, 68, 68, 0.1); cursor: pointer;' : 'cursor: pointer;';
        el('card-player').setAttribute('style', `display: block; ${trackClass}`);
        el('card-player').setAttribute('onclick', `window.chrome.webview.postMessage('track:${state.player.id}')`);
        text('val-player-name', state.player.name || '?');
        text('val-player-id', state.player.id || 0);
        text('val-player-hp', (state.player.hp || 0) + ' / ' + (state.player.hpMax || 0) + ' (' + (state.player.hpPercent || 0) + '%)');
        if (el('val-player-apmp')) {
            el('val-player-apmp').innerHTML = `<span style="color:#3b82f6;">${state.player.ap || 0}</span> <span style="color:#888;">/</span> <span style="color:#10b981;">${state.player.mp || 0}</span>`;
        }
        text('val-player-shield', state.player.shield || 0);
        text('val-player-cell', state.player.cellId || 0);
    } else {
        showBlock('card-player', false);
    }

    if (state.mapInfo) {
        showBlock('card-map', true);
        text('val-map-id', state.mapInfo.id || 0);
        text('val-map-nameid', state.mapInfo.nameId || 0);
        text('val-map-subareaid', state.mapInfo.subAreaId || 0);
        text('val-map-worldmap', state.mapInfo.worldMap || 0);
        text('val-map-pos', (state.mapInfo.posX || 0) + ', ' + (state.mapInfo.posY || 0));
    } else {
        showBlock('card-map', false);
    }

    if (state.isInFight) {
        showBlock('card-combat', true);
        text('val-combat-turn', state.isPlayerTurn ? 'YES' : 'NO');
        if(el('val-combat-turn')) el('val-combat-turn').className = 'stat-value ' + (state.isPlayerTurn ? 'active' : 'inactive');
        
        if (el('val-combat-turn-id')) {
            el('val-combat-turn-id').innerText = state.currentTurnEntityId || '?';
            if (state.currentTurnEntityId) {
                let trackClass = (state.currentTurnEntityId === state.trackedEntityId) ? 'color: #ef4444; cursor: pointer;' : 'cursor: pointer;';
                el('val-combat-turn-id').setAttribute('style', trackClass);
                el('val-combat-turn-id').setAttribute('onclick', `window.chrome.webview.postMessage('track:${state.currentTurnEntityId}')`);
            }
        }
    } else {
        showBlock('card-combat', false);
    }

    const entities = state.isInFight ? state.fightEntities : state.mapEntities;
    if (entities && entities.length > 0) {
        showBlock('card-entities', true);
        text('val-entities-count', 'ENTITIES (' + entities.length + ')');
        
        const list = el('list-entities');
        const scrollPos = list.scrollTop;
        
        let html = '';
        for (const e of entities) {
            let teamClass = '';
            if (state.isInFight) teamClass = (e.teamId === 1) ? 'team-ally' : (e.teamId >= 0 ? 'team-enemy' : '');
            
            let trackClass = (e.id === state.trackedEntityId) ? 'border: 1px solid #ef4444; background: rgba(239, 68, 68, 0.1);' : '';
            
            html += `<div class="entity-item ${teamClass}" style="cursor: pointer; ${trackClass}" onclick="window.chrome.webview.postMessage('track:${e.id}')">
                <div style="min-width: 0; flex: 1;">
                    <div class="entity-name">Type ${e.type}</div>
                    <div class="stat-label" style="font-size:10px;">ID: ${e.id}</div>
                </div>
                <div style="text-align: right; min-width: 0; flex: 1;">
                    <div class="entity-cell">Cell ${e.cellId}</div>
                    <div class="stat-value ${e.isActive ? 'active' : 'inactive'}" style="font-size: 10px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;" title="${e.animationState}">${e.animationState}</div>
                </div>
            </div>`;
        }
        list.innerHTML = html;
        list.scrollTop = scrollPos;
    } else {
        showBlock('card-entities', false);
    }

    if (state.portals && state.portals.length > 0) {
        showBlock('card-portals', true);
        let html = '';
        for (const p of state.portals) {
            let trackClass = (p.id === state.trackedEntityId) ? 'border: 1px solid #ef4444; background: rgba(239, 68, 68, 0.1);' : '';
            let reason = p.closedReason || 0;
            let open = reason === 0;
            let label = open ? 'OPEN' : (reason === 1 ? 'CLOSED (entity)' : 'CLOSED (used)');
            html += `<div class="portal-item" style="cursor: pointer; ${trackClass}" onclick="window.chrome.webview.postMessage('track:${p.id}')">
                <span class="portal-index">#${p.index}</span>
                <span style="font-size:10px; color:#80848e;">ID: ${p.id}</span>
                <span class="portal-cell">Cell ${p.cellId}</span>
                <span class="${open ? 'portal-open' : 'portal-closed'}">${label}</span>
            </div>`;
        }
        el('list-portals').innerHTML = html;
    } else {
        showBlock('card-portals', false);
    }
}

function updateState(data) {
    if (!initialized) {
        initialized = true;
    }
    
    try {
        const payload = JSON.parse(data);

        if (payload.ui) applyUi(payload.ui);

        if (!payload.state) return;

        const state = payload.state;
        currentConfig = payload.config;

        const lang = currentConfig.language || 'en';
        if (lang !== appliedLang) {
            appliedLang = lang;
            applyI18n(lang);
            lastKeybindsSignature = '';
            document.querySelectorAll('#lang-segment .segment').forEach(s =>
                s.classList.toggle('active', s.dataset.val === lang));
        }

        updateDebugView(state);
        
        if (el('toggle-grid')) el('toggle-grid').checked = currentConfig.gridOverlay;
        if (el('grid-color-picker')) el('grid-color-picker').value = currentConfig.gridColor || '#ffffff';
        if (el('grid-opacity')) {
            el('grid-opacity').value = currentConfig.gridOpacity ?? 100;
            el('grid-opacity-val').innerText = el('grid-opacity').value + '%';
        }
        if (currentConfig.gridMode) {
            document.querySelectorAll('#grid-mask-segment .segment[data-type="mode"]').forEach(s => {
                if (s.dataset.val === currentConfig.gridMode) s.classList.add('active');
                else s.classList.remove('active');
            });
        }
        
        const losSeg = document.querySelector('#grid-mask-segment .segment[data-val="los"]');
        if (losSeg) {
            if (currentConfig.gridShowLos) losSeg.classList.add('active');
            else losSeg.classList.remove('active');
        }

        const moveSeg = document.querySelector('#grid-mask-segment .segment[data-val="move"]');
        if (moveSeg) {
            if (currentConfig.gridShowMove) moveSeg.classList.add('active');
            else moveSeg.classList.remove('active');
        }
        
        if (el('grid-fill-opacity')) {
            el('grid-fill-opacity').value = currentConfig.gridFillOpacity ?? 60;
            el('fill-opacity-val').innerText = el('grid-fill-opacity').value + '%';
        }
        if (el('grid-fill-color')) {
            el('grid-fill-color').value = currentConfig.gridFillColor || '#555555';
        }
        if (el('bg-overlay')) {
            el('bg-overlay').value = currentConfig.backgroundOverlay ?? 0;
            el('bg-overlay-val').innerText = el('bg-overlay').value + '%';
        }
        if (el('toggle-los-walls')) el('toggle-los-walls').checked = currentConfig.losShowWalls ?? true;
        if (el('los-wall-color')) el('los-wall-color').value = currentConfig.losWallColor || '#1a1a1f';
        if (el('los-wall-opacity')) {
            el('los-wall-opacity').value = currentConfig.losWallOpacity ?? 82;
            el('los-wall-opacity-val').innerText = el('los-wall-opacity').value + '%';
        }
        if (el('los-even-color')) el('los-even-color').value = currentConfig.losEvenColor || '#4a4a4f';
        if (el('los-even-opacity')) {
            el('los-even-opacity').value = currentConfig.losEvenOpacity ?? 60;
            el('los-even-opacity-val').innerText = el('los-even-opacity').value + '%';
        }
        if (el('los-odd-color')) el('los-odd-color').value = currentConfig.losOddColor || '#2e2e33';
        if (el('los-odd-opacity')) {
            el('los-odd-opacity').value = currentConfig.losOddOpacity ?? 68;
            el('los-odd-opacity-val').innerText = el('los-odd-opacity').value + '%';
        }
        if (el('portal-color-1')) el('portal-color-1').value = currentConfig.portalColor1 || '#5b21b6';
        if (el('portal-color-2')) el('portal-color-2').value = currentConfig.portalColor2 || '#7c5cd6';
        if (el('portal-color-3')) el('portal-color-3').value = currentConfig.portalColor3 || '#a78bfa';
        if (el('portal-color-4')) el('portal-color-4').value = currentConfig.portalColor4 || '#c4b5fd';
        if (el('portal-opacity')) {
            el('portal-opacity').value = currentConfig.portalOpacity ?? 100;
            el('portal-opacity-val').innerText = el('portal-opacity').value + '%';
        }
        if (el('portal-thickness')) {
            el('portal-thickness').value = currentConfig.portalThickness ?? 2;
            el('portal-thickness-val').innerText = el('portal-thickness').value + 'px';
        }
        if (el('portal-filled')) el('portal-filled').checked = currentConfig.portalFilled ?? false;
        if (el('closed-color')) el('closed-color').value = currentConfig.closedPortalColor || '#73737f';
        if (el('closed-filled')) el('closed-filled').checked = currentConfig.closedPortalFilled ?? false;
        if (el('connector-color')) el('connector-color').value = currentConfig.connectorColor || '#a78bfa';
        if (el('connector-opacity')) el('connector-opacity').value = currentConfig.connectorOpacity ?? 100;
        if (el('connector-thickness')) el('connector-thickness').value = currentConfig.connectorThickness ?? 3;
        if (el('portal-show-number')) el('portal-show-number').checked = currentConfig.portalShowNumber ?? true;
        if (el('portal-number-color')) el('portal-number-color').value = currentConfig.portalNumberColor || '#ffffff';
        if (el('portal-number-ox')) el('portal-number-ox').value = currentConfig.portalNumberOffsetX ?? 0;
        if (el('portal-number-oy')) el('portal-number-oy').value = currentConfig.portalNumberOffsetY ?? 0;
        if (el('portal-number-scale')) {
            el('portal-number-scale').value = currentConfig.portalNumberScale ?? 100;
            el('portal-number-scale-val').innerText = el('portal-number-scale').value + '%';
        }
        if (currentConfig.portalNumberShape) {
            document.querySelectorAll('#portal-shape-segment .segment').forEach(s =>
                s.classList.toggle('active', s.dataset.val === currentConfig.portalNumberShape));
        }
        if (el('portal-grey-deleted')) el('portal-grey-deleted').checked = currentConfig.portalGreyDeleted ?? false;
        if (el('portal-show-distance')) el('portal-show-distance').checked = currentConfig.portalShowDistance ?? false;
        if (el('damage-show')) el('damage-show').checked = currentConfig.damageShow ?? true;
        if (el('damage-color')) el('damage-color').value = currentConfig.damageColor || '#0e7490';
        if (el('damage-thickness')) el('damage-thickness').value = currentConfig.damageThickness ?? 6;
        if (el('damage-scale')) {
            el('damage-scale').value = currentConfig.damageScale ?? 100;
            el('damage-scale-val').innerText = el('damage-scale').value + '%';
        }
        if (el('damage-outline')) el('damage-outline').checked = currentConfig.damageOutline ?? true;
        if (el('damage-outline-color')) el('damage-outline-color').value = currentConfig.damageOutlineColor || '#000000';
        if (el('damage-outline-thickness')) el('damage-outline-thickness').value = currentConfig.damageOutlineThickness ?? 2;
        if (el('poll-slider')) {
            el('poll-slider').value = currentConfig.memoryPollRate ?? 250;
            el('poll-val').innerText = el('poll-slider').value + 'ms';
        }
        if (el('entrance-thickness')) el('entrance-thickness').value = currentConfig.entranceThickness ?? 2;
        if (el('entrance-opacity')) el('entrance-opacity').value = currentConfig.entranceOpacity ?? 50;
        if (el('entrance-color')) el('entrance-color').value = currentConfig.entranceColor || '#fbbf24';
        if (el('entrance-filled')) el('entrance-filled').checked = currentConfig.entranceFilled ?? true;
        
        if (el('statusDot')) {
            el('statusDot').style.background = state.isInGame ? '#23a559' : '#ef4444';
            el('statusDot').style.boxShadow = state.isInGame ? '0 0 8px rgba(35,165,89,0.5)' : '0 0 8px rgba(239,68,68,0.5)';
        }

        if (payload.keybinds) renderKeybinds(payload.keybinds, payload.capturingId);
    } catch(e) {
        console.error("Erreur parsing JSON:", e);
    }
}

if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener('message', e => updateState(e.data));
    window.chrome.webview.postMessage('ready');
}

el('header').addEventListener('mousedown', e => {
    if (!e.target.closest('#collapseBtn')) {
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage('drag');
        }
    }
});

el('collapseBtn').addEventListener('click', e => {
    e.stopPropagation();
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage('toggle');
    }
});

const exitBtn = document.getElementById('exit-btn');
if (exitBtn) {
    exitBtn.addEventListener('click', () => {
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage('exit');
        }
    });
}

(function setupTooltips() {
    const tip = el('ui-tooltip');
    if (!tip) return;
    let current = null;

    const place = host => {
        const r = host.getBoundingClientRect();
        let x = r.left + r.width / 2;
        const half = tip.offsetWidth / 2;
        x = Math.max(half + 4, Math.min(window.innerWidth - half - 4, x));
        tip.style.left = x + 'px';
        tip.style.top = r.top + 'px';
    };

    document.addEventListener('mouseover', e => {
        const host = e.target.closest('[data-tip]');
        if (!host || host === current) return;
        current = host;
        tip.textContent = host.getAttribute('data-tip');
        tip.classList.add('show');
        place(host);
    });

    document.addEventListener('mouseout', e => {
        const host = e.target.closest('[data-tip]');
        if (!host) return;
        if (e.relatedTarget && host.contains(e.relatedTarget)) return;
        current = null;
        tip.classList.remove('show');
    });
})();

applyI18n('en');
