const iconMinus = '<path d="M19 13H5v-2h14v2z"/>';
const iconPlus = '<path d="M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z"/>';

let currentConfig = { gridOverlay: true };
let initialized = false;

const el = id => document.getElementById(id);
const text = (id, val) => { if (el(id)) el(id).innerText = val; };
const show = (id, visible) => { if (el(id)) el(id).style.display = visible ? 'flex' : 'none'; };
const showBlock = (id, visible) => { if (el(id)) el(id).style.display = visible ? 'block' : 'none'; };

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
            html += `<div class="portal-item" style="cursor: pointer; ${trackClass}" onclick="window.chrome.webview.postMessage('track:${p.id}')">
                <span class="portal-index">#${p.index}</span>
                <span style="font-size:10px; color:#80848e;">ID: ${p.id}</span>
                <span class="portal-cell">Cell ${p.cellId}</span>
                <span class="${p.isOpen ? 'portal-open' : 'portal-closed'}">${p.isOpen ? 'OPEN' : 'CLOSED'}</span>
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
        const state = payload.state;
        currentConfig = payload.config;

        updateDebugView(state);
        
        if (el('toggle-grid')) el('toggle-grid').checked = currentConfig.gridOverlay;
        if (el('grid-color-picker')) el('grid-color-picker').value = currentConfig.gridColor || '#ffffff';
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
        
        if (el('statusDot')) {
            el('statusDot').style.background = state.isInGame ? '#23a559' : '#ef4444';
            el('statusDot').style.boxShadow = state.isInGame ? '0 0 8px rgba(35,165,89,0.5)' : '0 0 8px rgba(239,68,68,0.5)';
        }
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
    document.body.classList.toggle('collapsed');
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage('toggle');
    }
    const icon = el('collapseIcon');
    icon.innerHTML = document.body.classList.contains('collapsed') ? iconPlus : iconMinus;
});

const exitBtn = document.getElementById('exit-btn');
if (exitBtn) {
    exitBtn.addEventListener('click', () => {
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage('exit');
        }
    });
}
