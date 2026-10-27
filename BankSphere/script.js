
const $ = (id) => document.getElementById(id);

let accounts = [];
let transactions = [];
let noticeTimer = null;

function money(value) {
  return "₹" + Number(value || 0).toLocaleString("en-IN", {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2
  });
}

function escapeHTML(value) {
  return String(value ?? "").replace(/[&<>"']/g, ch => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;",
    '"': "&quot;", "'": "&#39;"
  })[ch]);
}

function showNotice(message, success) {
  const box = $("notice");
  box.textContent = message;
  box.className = "notice " + (success ? "success" : "error");
  box.hidden = false;
  if (noticeTimer) clearTimeout(noticeTimer);
  noticeTimer = setTimeout(() => { box.hidden = true; }, 5000);
}

function showPage(name) {
  document.querySelectorAll(".page").forEach(page => {
    page.hidden = page.id !== "page-" + name;
  });

  document.querySelectorAll(".nav-item").forEach(button => {
    button.classList.toggle("active", button.dataset.page === name);
  });

  if (name === "accounts") renderAccounts();
  if (name === "transactions") renderTransactions();
  if (name === "deposit" || name === "withdraw" || name === "transfer") {
    renderAccountSelects();
  }
}

document.querySelectorAll("[data-page]").forEach(button => {
  button.addEventListener("click", () => showPage(button.dataset.page));
});

document.querySelectorAll("[data-go]").forEach(button => {
  button.addEventListener("click", () => showPage(button.dataset.go));
});

async function api(path, method = "GET", data = null) {
  const options = { method, headers: {} };

  if (data) {
    options.headers["Content-Type"] = "application/x-www-form-urlencoded";
    options.body = new URLSearchParams(data).toString();
  }

  const response = await fetch(path, options);
  if (!response.ok) throw new Error("HTTP error " + response.status);
  return response.json();
}

async function refreshData() {
  try {
    const [accountResult, transactionResult] = await Promise.all([
      api("/api/accounts"),
      api("/api/transactions")
    ]);

    accounts = accountResult.accounts || [];
    transactions = transactionResult.transactions || [];

    renderDashboard();
    renderAccounts();
    renderTransactions();
    renderAccountSelects();
  } catch (error) {
    showNotice(
      "Cannot connect to the C++ server. Make sure BankSphere.exe is running.",
      false
    );
  }
}

function renderDashboard() {
  $("stat-accounts").textContent = accounts.length;
  $("stat-transactions").textContent = transactions.length;

  const total = accounts.reduce((sum, account) =>
    sum + Number(account.balance), 0);
  $("stat-balance").textContent = money(total);

  const recent = accounts.slice(-5).reverse();
  $("recent-accounts").innerHTML = recent.length
    ? recent.map(account => `
      <tr>
        <td>#${account.id}</td>
        <td>${escapeHTML(account.name)}</td>
        <td>${escapeHTML(account.phone)}</td>
        <td><strong>${money(account.balance)}</strong></td>
      </tr>`).join("")
    : `<tr><td colspan="4" class="empty">No accounts yet. Create your first account.</td></tr>`;
}

function renderAccounts() {
  const search = ($("account-search")?.value || "").toLowerCase().trim();

  const filtered = accounts.filter(account =>
    String(account.id).includes(search) ||
    account.name.toLowerCase().includes(search) ||
    account.phone.toLowerCase().includes(search)
  );

  $("accounts-table").innerHTML = filtered.length
    ? filtered.map(account => `
      <tr>
        <td>#${account.id}</td>
        <td>${escapeHTML(account.name)}</td>
        <td>${escapeHTML(account.phone)}</td>
        <td><strong>${money(account.balance)}</strong></td>
      </tr>`).join("")
    : `<tr><td colspan="4" class="empty">No matching accounts found.</td></tr>`;
}

function renderAccountSelects() {
  document.querySelectorAll(".account-select").forEach(select => {
    const previous = select.value;
    const isSource = select.name === "fromId";
    const placeholder = isSource ? "Select source account" : "Select account";

    select.innerHTML = `<option value="">${placeholder}</option>` +
      accounts.map(account => `
        <option value="${account.id}">
          ${account.id} — ${escapeHTML(account.name)} (${money(account.balance)})
        </option>`).join("");

    if (accounts.some(account => String(account.id) === previous)) {
      select.value = previous;
    }
  });
}

function renderTransactions() {
  const rows = [...transactions].reverse();

  $("transactions-table").innerHTML = rows.length
    ? rows.map(tx => `
      <tr>
        <td>#${tx.id}</td>
        <td>${escapeHTML(tx.type)}</td>
        <td>${tx.fromId ? "#" + tx.fromId : "—"}</td>
        <td>${tx.toId ? "#" + tx.toId : "—"}</td>
        <td><strong>${money(tx.amount)}</strong></td>
        <td>${escapeHTML(tx.time)}</td>
      </tr>`).join("")
    : `<tr><td colspan="6" class="empty">No transactions recorded in this session.</td></tr>`;
}

async function submitForm(formId, endpoint, successPage) {
  const form = $(formId);
  const button = form.querySelector('button[type="submit"]');
  const oldText = button.textContent;
  button.disabled = true;
  button.textContent = "Processing...";

  try {
    const data = Object.fromEntries(new FormData(form).entries());
    const result = await api(endpoint, "POST", data);

    showNotice(result.message || "Request completed.", result.ok);

    if (result.ok) {
      form.reset();
      await refreshData();
      if (successPage) showPage(successPage);
    }
  } catch (error) {
    showNotice("Request failed. Check that the C++ server is running.", false);
  } finally {
    button.disabled = false;
    button.textContent = oldText;
  }
}

$("add-form").addEventListener("submit", event => {
  event.preventDefault();
  submitForm("add-form", "/api/add", "accounts");
});

$("deposit-form").addEventListener("submit", event => {
  event.preventDefault();
  submitForm("deposit-form", "/api/deposit", "accounts");
});

$("withdraw-form").addEventListener("submit", event => {
  event.preventDefault();
  submitForm("withdraw-form", "/api/withdraw", "accounts");
});

$("transfer-form").addEventListener("submit", event => {
  event.preventDefault();
  submitForm("transfer-form", "/api/transfer", "accounts");
});

$("account-search").addEventListener("input", renderAccounts);
$("refresh-accounts").addEventListener("click", refreshData);
$("refresh-transactions").addEventListener("click", refreshData);

refreshData();
