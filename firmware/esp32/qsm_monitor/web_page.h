/**
 * @file  web_page.h
 * @brief 内嵌网页（不依赖任何 CDN，断网也能用）
 *
 * 设计：单页 + 1 s 轮询 /api/state，前端自己维护最近 120 个点画趋势条。
 * 之所以不用 WebSocket，是为了让整页逻辑能塞进几十行 JS，便于现场排障。
 *
 * 文案全部中英双语：中文为主行，英文为灰色小字副行（class="en"）。
 */
#ifndef QSM_WEB_PAGE_H
#define QSM_WEB_PAGE_H

static const char QSM_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="zh-CN"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>排队状态监测器 Queue Status Monitor</title>
<style>
:root{--few:#2e7d32;--half:#f9a825;--full:#c62828;--bg:#f5f6f8;--card:#fff;--tx:#222;--mut:#777}
*{box-sizing:border-box}
body{margin:0;font-family:-apple-system,"Segoe UI","Microsoft YaHei",sans-serif;background:var(--bg);color:var(--tx)}
.wrap{max-width:760px;margin:0 auto;padding:16px}
h1{font-size:18px;margin:8px 0 14px;font-weight:600}
h1 .en{font-size:12px;font-weight:400;color:var(--mut);letter-spacing:.3px}
.card{background:var(--card);border-radius:12px;padding:16px;margin-bottom:14px;box-shadow:0 1px 3px rgba(0,0,0,.08)}
.grade{display:flex;align-items:center;gap:12px}
.dot{width:20px;height:20px;border-radius:50%;background:#999;transition:background .3s}
.gname{font-size:26px;font-weight:700;line-height:1.15}
.gen{font-size:13px;font-weight:600;color:var(--mut)}
.gsub{color:var(--mut);font-size:13px}
.en{display:block;font-size:11px;font-weight:400;color:var(--mut);margin-top:1px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(110px,1fr));gap:12px}
.kv .k{font-size:12px;color:var(--mut)}
.kv .v{font-size:20px;font-weight:600;font-variant-numeric:tabular-nums}
.kv .v.big{font-size:34px;line-height:1.1}
.pred{background:#eef4ff;border:1px solid #d6e2f7}
.pred .v{color:#1a3d7c}
.bars{display:flex;align-items:flex-end;gap:2px;height:64px;margin-top:6px}
.bars i{flex:1;background:#cfd8dc;border-radius:2px 2px 0 0;transition:height .3s,background .3s}
.bd{color:#bbb;margin-right:5px;transition:color .2s}
.bds{font-size:15px;font-weight:600;color:var(--mut)}
.bds.on{color:#e8590c}
.alert{display:none;background:#ffebee;color:#b71c1c;border-left:4px solid #c62828;
  padding:10px 14px;border-radius:8px;margin-bottom:14px;font-weight:600}
.alert .en{color:#b71c1c;font-size:12px;font-weight:400}
.alert.on{display:block}
.stale{display:none;background:#fff4e5;color:#8a5300;border-left:4px solid #f0a020;
  padding:8px 12px;border-radius:8px;margin-bottom:12px;font-size:13px}
.stale.on{display:block}
.btn{font:inherit;font-size:14px;padding:9px 14px;margin:0 8px 8px 0;border:1px solid #c7d0dd;
  border-radius:8px;background:#f8fafc;color:#123;cursor:pointer}
.btn:active{background:#e8eef7}
.btn.warn{border-color:#f0b7b7;color:#a12622}
.hint{font-size:11px;color:var(--mut);margin-top:2px}
.foot{color:var(--mut);font-size:12px;text-align:center;padding:8px}
</style></head><body><div class="wrap">
<h1>排队状态监测器<span class="en">Queue Status Monitor</span></h1>
<div id="alert" class="alert">⚠️ 队列拥挤，建议稍后再来<span class="en">Queue is crowded — please come back later</span></div>
<div id="stale" class="stale">⏳ 数据已 <span id="staleS">0</span> 秒未更新 —— STM32 可能没在发帧<span class="en">No update for <span id="staleEnS">0</span> s — STM32 may be offline</span></div>

<div class="card grade">
  <div id="dot" class="dot"></div>
  <div>
    <div id="gname" class="gname">—</div>
    <div id="gnameEn" class="gen">—</div>
    <div id="gsub" class="gsub">等待数据…</div>
    <div id="gsubEn" class="en">waiting for data</div>
  </div>
</div>

<div class="card"><div class="grid">
  <div class="kv"><div class="k">在场人数<span class="en">People now</span></div><div id="occ" class="v">—</div></div>
  <div class="kv"><div class="k">累计进入<span class="en">Total in</span></div><div id="cin" class="v">—</div></div>
  <div class="kv"><div class="k">累计离开<span class="en">Total out</span></div><div id="cout" class="v">—</div></div>
  <div class="kv"><div class="k">50% 探头遮挡<span class="en">50% sensor blocked</span></div><div id="q50" class="v">—</div></div>
  <div class="kv"><div class="k">100% 探头遮挡<span class="en">100% sensor blocked</span></div><div id="q100" class="v">—</div></div>
</div></div>

<div class="card">
  <div class="k" style="font-size:12px;color:#777">四路探头实时状态（挥手试每一支）<span class="en">Live sensor status — wave at each sensor</span></div>
  <div class="grid">
    <div class="kv"><div class="k"><span id="bA" class="bd">●</span>入口 A<span class="en">Entry A</span></div><div id="bAs" class="bds">—</div></div>
    <div class="kv"><div class="k"><span id="bB" class="bd">●</span>入口 B<span class="en">Entry B</span></div><div id="bBs" class="bds">—</div></div>
    <div class="kv"><div class="k"><span id="b1" class="bd">●</span>队列 50%<span class="en">Queue 50%</span></div><div id="b1s" class="bds">—</div></div>
    <div class="kv"><div class="k"><span id="b2" class="bd">●</span>队列 100%<span class="en">Queue 100%</span></div><div id="b2s" class="bds">—</div></div>
  </div>
</div>

<div class="card pred"><div class="grid">
  <div class="kv"><div class="k">预测排队人数<span class="en">Predicted queue length</span></div>
    <div id="pred" class="v big">—</div></div>
  <div class="kv"><div class="k">参考天数（近 7 天同时段）<span class="en">Based on (same slot, last 7 days)</span></div>
    <div class="v"><span id="predn">—</span><span id="prednEn" class="en"></span></div></div>
</div></div>

<div class="card">
  <div class="k" style="font-size:12px;color:#777">最近 2 分钟分级趋势<span class="en">Grade trend, last 2 minutes</span></div>
  <div id="bars" class="bars"></div>
</div>

<div class="card">
  <div class="k" style="font-size:12px;color:#777">复位（不需要碰板子）<span class="en">Reset — no need to touch the board</span></div>
  <button class="btn" onclick="cmd(1)">清零计数<span class="en">Reset counters</span></button>
  <button class="btn warn" onclick="cmd(2)">清空历史<span class="en">Clear history</span></button>
  <button class="btn warn" onclick="cmd(3)">全部复位<span class="en">Reset all</span></button>
  <div class="hint">清零计数 = 进出人数与在场人数归零；清空历史 = 删除 SD 卡最近 8 天的记录并重新积累<span class="en">Counters: set in/out/people to 0. History: delete the last 8 daily files on the SD card.</span></div>
</div>

<div class="foot"><span id="meta">—</span></div>
</div>
<script>
const NAMES=["较少","约半","全满"], NAMES_EN=["Few","About half","Full"];
const COLORS=["var(--few)","var(--half)","var(--full)"];
const hist=[]; const MAXP=120;
function S(id,zh,en){
  var a=document.getElementById(id); if(a) a.textContent=zh;
  var b=document.getElementById(id+'En'); if(b) b.textContent=en;
}
function fmtMs(ms){ if(ms<1000) return ms+" ms"; return (ms/1000).toFixed(1)+" s"; }
async function cmd(n){
  try{
    await fetch('/api/reset?cmd='+n,{cache:'no-store'});
    document.getElementById('meta').textContent='已发送复位命令 / reset cmd sent: '+n;
  }catch(e){
    document.getElementById('meta').textContent='复位命令发送失败 / reset failed';
  }
}
async function tick(){
  try{
    const r=await fetch('/api/state',{cache:'no-store'});
    const d=await r.json();
    if(!d.valid){
      S('gsub','未收到 STM32 数据','no data from STM32');
      document.getElementById('stale').classList.add('on');
      return;
    }
    const g=Math.max(0,Math.min(2,d.grade|0));
    document.getElementById('dot').style.background=COLORS[g];
    S('gname',NAMES[g],NAMES_EN[g]);
    const hasTime=d.datetime && d.datetime!=='-';
    S('gsub',hasTime?d.datetime:'RTC 未就绪',hasTime?d.datetime:'RTC not ready');
    document.getElementById('occ').textContent=d.occ;
    document.getElementById('cin').textContent=d.in;
    document.getElementById('cout').textContent=d.out;
    document.getElementById('q50').textContent=fmtMs(d.q50);
    document.getElementById('q100').textContent=fmtMs(d.q100);
    const pn=d.predn|0;
    document.getElementById('pred').textContent=pn>0?d.pred:'—';
    S('predn',pn>0?pn+' 天':'历史不足',pn>0?pn+' day(s)':'not enough history');

    // 四路探头实时状态
    const bm=d.beams|0, bid=['bA','bB','b1','b2'];
    for(let i=0;i<4;i++){
      const on=((bm>>i)&1)===1;
      const dot=document.getElementById(bid[i]);
      const st=document.getElementById(bid[i]+'s');
      if(dot) dot.style.color=on?'#e8590c':'#bbb';
      if(st){ st.textContent=on?'遮挡':'通光'; st.className='bds'+(on?' on':''); }
    }

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

    // 断流提示：超过 2.5 s 没有新帧就亮橙色条，避免盯着一个"冻住"的页面
    const age=(d.age==4294967295)?99999999:(d.age|0);
    const stale=age>2500;
    document.getElementById('stale').classList.toggle('on',stale);
    if(stale){
      const s=(age/1000).toFixed(0);
      document.getElementById('staleS').textContent=s;
      document.getElementById('staleEnS').textContent=s;
    }

    document.getElementById('meta').textContent=
      'good '+d.good+' · bad '+d.bad+' · age '+(d.age==4294967295?'—':(d.age/1000).toFixed(1)+' s');
  }catch(e){
    S('gsub','连接中断','disconnected');
    document.getElementById('stale').classList.add('on');
  }
}
tick(); setInterval(tick,400);
</script></body></html>
)HTML";

#endif /* QSM_WEB_PAGE_H */
