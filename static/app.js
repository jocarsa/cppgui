const API = '/api/customers';
const table = document.querySelector('#custTable tbody');
const form  = document.getElementById('custForm');
const toast = document.getElementById('toast');
const cancel= document.getElementById('cancelBtn');
let editing = null;

//──────── CSV helpers ────────
const csv2array = txt => {
  const [header,...rows] = txt.trim().split('\n').map(l=>l.split(','));
  return rows.map(r=>Object.fromEntries(r.map((v,i)=>[header[i],v])));
};
const obj2url  = o => new URLSearchParams(o).toString();

//──────── UI helpers ─────────
const show = (msg, ok=true) => {
  toast.textContent=msg; toast.className=ok?'ok':'err'; toast.hidden=false;
  setTimeout(()=>toast.hidden=true,2000);
};

const list = async () => {
  const res = await fetch(API);
  const data = csv2array(await res.text());
  table.innerHTML = data.map(c=>`
    <tr>
      <td>${c.name}</td><td>${c.surname}</td>
      <td>${c.email}</td><td>${c.phone}</td>
      <td>
        <button data-e="${c.id}">✏️</button>
        <button data-d="${c.id}">🗑️</button>
      </td>
    </tr>`).join('');
};

//──────── CRUD actions ───────
const save = async data => {
  const method = editing ? 'PUT' : 'POST';
  const url    = editing ? `${API}/${editing}` : API;
  const res = await fetch(url,{method,
            headers:{'Content-Type':'application/x-www-form-urlencoded'},
            body: obj2url(data)});
  show(res.ok? 'Saved':'Error',res.ok);
  editing=null; form.reset(); cancel.hidden=true; await list();
};

const remove = async id => {
  const res = await fetch(`${API}/${id}`,{method:'DELETE'});
  show(res.ok?'Deleted':'Error',res.ok); await list();
};

//──────── events ─────────────
form.addEventListener('submit',e=>{
  e.preventDefault();
  const d = Object.fromEntries(new FormData(form));
  delete d.id;
  save(d);
});
cancel.addEventListener('click',()=>{
  editing=null; form.reset(); cancel.hidden=true;
  document.getElementById('formTitle').textContent='Add customer';
});
table.addEventListener('click',e=>{
  if(e.target.dataset.e){                // edit
    const id=e.target.dataset.e;
    const cells=e.target.closest('tr').children;
    form.name.value    = cells[0].textContent;
    form.surname.value = cells[1].textContent;
    form.email.value   = cells[2].textContent;
    form.phone.value   = cells[3].textContent;
    editing=id; cancel.hidden=false;
    document.getElementById('formTitle').textContent='Edit customer';
  }else if(e.target.dataset.d){          // delete
    if(confirm('Delete this customer?'))
      remove(e.target.dataset.d);
  }
});

// start
list();

