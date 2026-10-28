/**
 * @file  web_page.h
 * @brief 内嵌网页（不依赖任何 CDN，断网也能用）
 *
 * 设计：单页 + 1 s 轮询 /api/state，前端自己维护最近 120 个点画趋势条。
 * 之所以不用 WebSocket，是为了让整页逻辑能塞进几十行 JS，便于现场排障。
 */
#ifndef QSM_WEB_PAGE_H
#define QSM_WEB_PAGE_H

static const char QSM_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="zh-CN"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>排队状态监测器</title>
<style>
:root{--few:#2e7d32;--half:#f9a825;--full:#c62828;--bg:#f5f6f8;--card:#fff;--tx:#222;--mut:#777}
*{box-sizing:border-box}
body{margin:0;font-family:-apple-system,"Segoe UI","Microsoft YaHei",sans-serif;background:var(--bg);color:var(--tx)}
.wrap{max-width:760px;margin:0 auto;padding:16px}
h1{font-size:18px;margin:8px 0 14px;font-weight:600}
.card{background:var(--card);border-radius:12px;padding:16px;margin-bottom:14px;box-shadow:0 1px 3px rgba(0,0,0,.08)}
.grade{display:flex;align-items:center;gap:12px}
.dot{width:20px;height:20px;border-radius:50%;background:#999;transition:background .3s}
.gname{font-size:26px;font-weight:700}
.gsub{color:var(--mut);font-size:13px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(110px,1fr));gap:12px}
.kv .k{font-size:12px;color:var(--mut)}
.kv .v{font-size:20px;font-weight:600;font-variant-numeric:tabular-nums}
.bars{display:flex;align-items:flex-end;gap:2px;height:64px;margin-top:6px}
.bars i{flex:1;background:#cfd8dc;border-radius:2px 2px 0 0;transition:height .3s,background .3s}
.alert{display:none;background:#ffebee;color:#b71c1c;border-left:4px solid #c62828;
  padding:10px 14px;border-radius:8px;margin-bottom:14px;font-weight:600}
.alert.on{display:block}
.foot{color:var(--mut);font-size:12px;text-align:center;padding:8px}
</style></head><body><div class="wrap">
<h1>排队状态监测器</h1>
<div id="alert" class="alert">⚠️ 队列拥挤，建议稍后再来</div>

<div class="card grade">
  <div id="dot" class="dot"></div>
  <div>
    <div id="gname" class="gname">—</div>
    <div id="gsub" class="gsub">等待数据…</div>
  </div>
</div>

<div class="card"><div class="grid">
  <div class="kv"><div class="k">在场人数</div><div id="occ" class="v">—</div></div>
  <div class="kv"><div class="k">累计进入</div><div id="cin" class="v">—</div></div>
  <div class="kv"><div class="k">累计离开</div><div id="cout" class="v">—</div></div>
  <div class="kv"><div class="k">50% 探头遮挡</div><div id="q50" class="v">—</div></div>
  <div class="kv"><div class="k">100% 探头遮挡</div><div id="q100" class="v">—</div></div>
</div></div>

<div class="card">
  <div class="k" style="font-size:12px;color:#777">最近 2 分钟分级趋势</div>
  <div id="bars" class="bars"></div>
</div>

<div class="foot"><span id="meta">—</span></div>
</div>
<script>
const NAMES=["较少","约半","全满"], COLORS=["var(--few)","var(--half)","var(--full)"];
const hist=[]; const MAXP=120;
function fmtMs(ms){ if(ms<1000) return ms+" ms"; return (ms/1000).toFixed(1)+" s"; }
async function tick(){
  try{
    const r=await fetch('/api/state',{cache:'no-store'});
    const d=await r.json();
    if(!d.valid){
      document.getElementById('gsub').textContent='未收到 STM32 数据';
      return;
    }
    const g=Math.max(0,Math.min(2,d.grade|0));
    document.getElementById('dot').style.background=COLORS[g];
    document.getElementById('gname').textContent=NAMES[g];
    document.getElementById('gsub').textContent=d.datetime && d.datetime!=='-' ? d.datetime : 'RTC 未就绪';
    document.getElementById('occ').textContent=d.occ;
    document.getElementById('cin').textContent=d.in;
    document.getElementById('cout').textContent=d.out;
    document.getElementById('q50').textContent=fmtMs(d.q50);
    document.getElementById('q100').textContent=fmtMs(d.q100);

    hist.push(g); if(hist.length>MAXP) hist.shift();
    const box=document.getElementById('bars');
    if(box.children.length!==hist.length){
      box.innerHTML=''; for(let i=0;i<hist.length;i++) box.appendChild(document.createElement('i'));
    }
    for(let i=0;i<hist.length;i++){
      const el=box.children[i];
      el.style.height=(18+hist[i]*41)+'%';
      el.style.background=COLORS[hist[i]];
    }

    const over=(g>=(d.alert_grade|0))||((d.occ|0)>=(d.alert_occ|0));
    document.getElementById('alert').classList.toggle('on',over);
    document.getElementById('meta').textContent=
      'good '+d.good+' · bad '+d.bad+' · age '+(d.age==4294967295?'—':(d.age/1000).toFixed(1)+' s');
  }catch(e){
    document.getElementById('gsub').textContent='连接中断';
  }
}
tick(); setInterval(tick,1000);
</script></body></html>
)HTML";

#endif /* QSM_WEB_PAGE_H */
