// Global State
let currentUser = null; // {id, name, tier, balance}
let role = 'guest';

// API
const API_URL = '/api';

function toggleLoginFields() {
    role = document.getElementById('user-role').value;
    const authDiv = document.getElementById('customer-auth');
    if (role === 'admin') authDiv.classList.add('d-none');
    else authDiv.classList.remove('d-none');
}

async function post(endpoint, data) {
    const res = await fetch(API_URL + endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
    });
    return await res.json();
}

async function login() {
    role = document.getElementById('user-role').value;
    const errorDiv = document.getElementById('login-error');
    errorDiv.classList.add('d-none');

    if (role === 'admin') {
        document.getElementById('login-page').classList.add('d-none');
        document.getElementById('admin-page').classList.remove('d-none');
        fetchState();
    } else {
        const id = document.getElementById('login-id').value;
        const pin = document.getElementById('login-pin').value;

        const res = await post('/login', { id: parseInt(id), pin: parseInt(pin) });

        if (res.status === 'ok') {
            currentUser = { id: parseInt(id), name: res.name, tier: res.tier, balance: res.balance };

            // Update UI
            document.getElementById('user-name').innerText = currentUser.name;
            document.getElementById('user-balance').innerText = '₹' + currentUser.balance.toFixed(2);
            document.getElementById('user-tier').innerText = getTierName(currentUser.tier);

            document.getElementById('login-page').classList.add('d-none');
            document.getElementById('customer-page').classList.remove('d-none');
            fetchState();
        } else {
            errorDiv.innerText = res.error || "Login Failed";
            errorDiv.classList.remove('d-none');
        }
    }
}

function logout() {
    location.reload();
}

// --- ADMIN ACTIONS ---

async function createCustomer() {
    const name = document.getElementById('new-name').value;
    const pin = document.getElementById('new-pin').value;
    const bal = document.getElementById('new-balance').value;
    const tier = document.getElementById('new-tier').value;

    const res = await post('/customer', {
        name: name,
        pin: parseInt(pin),
        balance: parseFloat(bal),
        tier: parseInt(tier)
    });

    const msg = document.getElementById('new-cust-msg');
    if (res.status === 'ok') {
        msg.innerText = `Created! Account #: ${res.account_number}`;
        // Clear inputs
        document.getElementById('new-name').value = '';
    } else {
        msg.innerText = "Error: " + res.error;
    }
}

async function processTx() {
    await post('/process', {});
}

async function unlockTx(id) {
    await post('/unlock', { id: id });
}

// --- CUSTOMER ACTIONS ---

async function createTx() {
    if (!currentUser) return;
    const receiver = document.getElementById('tx-receiver').value;
    const amt = document.getElementById('tx-amount').value;
    const urg = document.getElementById('tx-urgency').value;
    const pin = document.getElementById('tx-pin').value;
    const msg = document.getElementById('tx-msg');

    const res = await post('/transaction', {
        sender: currentUser.id,
        receiver: parseInt(receiver),
        amount: parseFloat(amt),
        urgency: parseInt(urg),
        pin: parseInt(pin)
    });

    if (res.status === 'ok') {
        msg.innerText = "Transfer Initiated!";
        msg.className = "mt-2 text-center text-success";
        document.getElementById('tx-amount').value = '';
        document.getElementById('tx-pin').value = '';
    } else {
        msg.innerText = res.error;
        msg.className = "mt-2 text-center text-danger";
    }
}

async function cancelTx(id) {
    await post('/cancel', { id: id });
}

// --- RENDERING ---

async function fetchState() {
    if (document.getElementById('login-page').classList.contains('d-none') === false) return; // Don't poll on login

    try {
        const res = await fetch(API_URL + '/state');
        const data = await res.json();
        render(data);
    } catch (e) {
        console.error("Connection lost", e);
    }
}

function getTierName(t) {
    if (t == 0) return 'Basic';
    if (t == 20) return 'Premium';
    if (t == 50) return 'VIP';
    if (t == 80) return 'Elite';
    return 'Unknown';
}

function render(data) {
    const now = Date.now() / 1000;

    // Filter Lists
    const waiting = data.transactions.filter(t => t.status === 0).sort((a, b) => b.effective_priority - a.effective_priority);
    const locked = data.transactions.filter(t => t.status === 1).sort((a, b) => a.unlock - b.unlock);

    // --- ADMIN VIEW ---
    if (role === 'admin') {
        document.getElementById('stat-pq').innerText = waiting.length;
        document.getElementById('stat-locked').innerText = locked.length;
        document.getElementById('stat-processed').innerText = data.stats.processed;

        // Render Customers
        if (data.customers) {
            const custBody = document.getElementById('all-customers-list');
            custBody.innerHTML = '';
            data.customers.forEach(c => {
                custBody.innerHTML += `<tr><td>${c.id}</td><td>${c.name}</td><td>${getTierName(c.tier)}</td><td>₹${c.balance.toFixed(2)}</td></tr>`;
            });
        }

        // Render Time Lock
        const lockContainer = document.getElementById('timelock-list');
        lockContainer.innerHTML = '';
        locked.forEach(t => {
            const timeLeft = Math.max(0, Math.round(t.unlock - now));
            lockContainer.innerHTML += `
                <div class="queue-item locked">
                    <div>
                        <strong>#${t.id}</strong> ₹${t.amount}<br>
                        <small class="text-secondary">${t.sender} &rarr; ${t.receiver}</small><br>
                        <small class="text-warning">Unlocks: ${timeLeft}s</small>
                    </div>
                    <div><button onclick="unlockTx(${t.id})" class="btn btn-sm btn-outline-warning">Force</button></div>
                </div>`;
        });

        // Render PQ
        const pqBody = document.getElementById('pq-list');
        pqBody.innerHTML = '';
        waiting.forEach((t, index) => {
            pqBody.innerHTML += `
                <tr>
                    <td>${index + 1}</td>
                    <td>${t.id}</td>
                    <td>${t.sender} &rarr; ${t.receiver}</td>
                    <td>₹${t.amount}</td>
                    <td class="text-info">${t.effective_priority.toFixed(1)}</td>
                    <td><span class="badge bg-primary">Waiting</span></td>
                </tr>`;
        });
    }

    // --- CUSTOMER VIEW ---
    if (role === 'customer' && currentUser) {
        // Find my transactions
        const mine = data.transactions.filter(t => t.sender === currentUser.id);

        // Refresh my balance logic (if API supported refreshing user data, currently purely front-ended. 
        // In real app, we'd GET /me periodically. For now, rely on login snapshot or infer?)
        // Let's stick to snapshot for now, or find self in customers list if data exposes it (Admin viewing all exposes it).
        // The /state API sends all customers? Yes I added that.
        const meRealtime = data.customers ? data.customers.find(c => c.id === currentUser.id) : null;
        if (meRealtime) {
            document.getElementById('user-balance').innerText = '₹' + meRealtime.balance.toFixed(2);
        }

        const list = document.getElementById('customer-tx-list');
        list.innerHTML = '';
        mine.forEach(t => {
            let status = ['Queued', 'Locked', 'Processing', 'Done', 'Cancelled'][t.status];
            let badge = ['primary', 'warning', 'info', 'success', 'secondary'][t.status];
            let cancelBtn = (t.status === 1) ? `<button onclick="cancelTx(${t.id})" class="btn btn-sm btn-danger">Cancel</button>` : '-';

            list.innerHTML += `
                <tr>
                    <td>${t.id}</td>
                    <td>${t.receiver}</td>
                    <td>₹${t.amount}</td>
                    <td><span class="badge bg-${badge}">${status}</span></td>
                    <td>${cancelBtn}</td>
                </tr>`;
        });
    }
}

setInterval(fetchState, 1000);
