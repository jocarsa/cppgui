/*  CSV ↔︎ object helpers  -------------------------------------------------- */
const parseCSV = txt => {
  const lines = txt.trim().split("\n");
  if (lines.length < 2) return [];
  const headers = lines[0].split(",");
  return lines.slice(1).map(line => {
    const cells = line.split(",");
    return Object.fromEntries(headers.map((h, i) => [h, cells[i]]));
  });
};

const toUrlEncoded = obj => new URLSearchParams(obj).toString();

/*  DOM references ---------------------------------------------------------- */
const customerForm  = document.getElementById("customer-form");  // ← real form
const customersList = document.getElementById("customers");
const idInput       = document.getElementById("customer-id");    // hidden <input>

/*  Back-end base path (matches the C++ code) ------------------------------- */
const API = "/api/customers";

/*  Populate <ul> ----------------------------------------------------------- */
const fetchCustomers = async () => {
  const res = await fetch(API);
  const csv = await res.text();
  const customers = parseCSV(csv);

  customersList.innerHTML = customers.map(c => `
      <li>
        ${c.name} ${c.surname} — ${c.email} — ${c.phone}
        <button onclick="editCustomer(${c.id},'${c.name}','${c.surname}','${c.email}','${c.phone}')">Edit</button>
        <button onclick="deleteCustomer(${c.id})">Delete</button>
      </li>
  `).join("");
};

/*  Button actions ---------------------------------------------------------- */
window.addCustomer = async () => {
  const formData = new FormData(customerForm);
  await fetch(API, {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: toUrlEncoded(Object.fromEntries(formData))
  });
  customerForm.reset();
  idInput.value = "";
  fetchCustomers();
};

window.updateCustomer = async () => {
  const id = idInput.value;
  if (!id) return alert("Select a customer to update first.");

  const formData = new FormData(customerForm);
  formData.delete("id");                // id goes in the URL, not in the body

  await fetch(`${API}/${id}`, {
    method: "PUT",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: toUrlEncoded(Object.fromEntries(formData))
  });
  customerForm.reset();
  idInput.value = "";
  fetchCustomers();
};

window.deleteCustomer = async id => {
  await fetch(`${API}/${id}`, { method: "DELETE" });
  fetchCustomers();
};

/*  Fill the form for editing ---------------------------------------------- */
window.editCustomer = (id, name, surname, email, phone) => {
  idInput.value        = id;
  customerForm.name.value    = name;
  customerForm.surname.value = surname;
  customerForm.email.value   = email;
  customerForm.phone.value   = phone;
};

/*  First load -------------------------------------------------------------- */
document.addEventListener("DOMContentLoaded", fetchCustomers);

