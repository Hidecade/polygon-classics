export function setupRanking(){
 const $=id=>document.getElementById(id), dialog=$('ranking-dialog');
 const buttons=[...dialog.querySelectorAll('button')].filter(button=>button.id!=='ranking-close');
 let pending=false;
 function render(data){
  const body=$('ranking-table').querySelector('tbody');body.replaceChildren();
  for(const row of data.rows){const tr=document.createElement('tr');for(const value of [row.rank,row.name,row.score.toLocaleString('ja-JP')]){const td=document.createElement('td');td.textContent=value;tr.append(td);}body.append(tr);}
  $('ranking-backups').replaceChildren(...data.backups.map(id=>{const option=document.createElement('option');option.value=id;option.textContent=new Date(Number(id.split('-')[1])).toLocaleString('ja-JP');return option;}));
  $('ranking-status').textContent=data.savedAt?`最終保存：${new Date(data.savedAt).toLocaleString('ja-JP')}`:'ゲーム標準のランキング（まだ保存されていません）';
 }
 async function request(value){
  const response=await fetch('/api/ranking',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(value)});
  const data=await response.json();if(!response.ok)throw new Error(data.error||'ランキングを操作できませんでした。');return data;
 }
 async function run(action){
  if(pending)return;pending=true;buttons.forEach(b=>b.disabled=true);
  try{await action();}catch(error){$('ranking-status').textContent=error.message;}
  finally{pending=false;buttons.forEach(b=>b.disabled=false);$('ranking-restore').disabled=!$('ranking-backups').value;}
 }
 $('ranking-menu').onclick=()=>{dialog.showModal();run(async()=>render(await request({action:'list'})));};
 $('ranking-close').onclick=()=>dialog.close();
 $('ranking-save').onclick=()=>run(async()=>render(await request({action:'save'})));
 $('ranking-backup').onclick=()=>run(async()=>{
  const result=await request({action:'backup'});
  const url=URL.createObjectURL(new Blob([JSON.stringify(result.value,null,2)],{type:'application/json'}));
  const link=document.createElement('a');link.href=url;link.download=result.id;link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
  render(await request({action:'list'}));$('ranking-status').textContent='バックアップを保存し、ファイルをダウンロードしました。';
 });
 $('ranking-restore').onclick=()=>{
  const id=$('ranking-backups').value;
  if(id&&confirm('選択したバックアップへランキングを戻しますか？ 現在の内容も自動バックアップします。'))run(async()=>render(await request({action:'restore',id})));
 };
 $('ranking-reset').onclick=()=>{
  if(confirm('ランキングをゲーム標準の状態に戻しますか？ 現在の内容は自動バックアップします。'))run(async()=>render(await request({action:'reset'})));
 };
 $('ranking-import').onclick=()=>$('ranking-file').click();
 $('ranking-file').onchange=()=>run(async()=>{
  const file=$('ranking-file').files[0];$('ranking-file').value='';if(!file)return;
  if(file.size>8192)throw new Error('バックアップファイルが大きすぎます。');
  const backup=JSON.parse(await file.text());
  if(confirm('このファイルからランキングを復元しますか？ 現在の内容も自動バックアップします。'))render(await request({action:'restore',backup}));
 });
}
