const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
class Element {
    constructor() { this.children=[]; this.value=''; this.disabled=false; this.required=false; this._text=''; }
    set textContent(value) { this._text=value; this.children=[]; this.value=''; }
    get textContent() { return this._text; }
    appendChild(child) { this.children.push(child); return child; }
}
async function main() {
    const html=fs.readFileSync(path.join(__dirname,'../main/eva_radio_setup.html'),'utf8');
    const source=html.match(/<script>([\s\S]*?)<\/script>/)[1];
    const elements=new Map();
    const get=id=>{ if(!elements.has(id))elements.set(id,new Element()); return elements.get(id); };
    let failure=false, empty=false, hold=null;
    const posts=[];
    const name='中文 \"WiFi\" <img onerror=alert(1)>';
    const sandbox={document:{getElementById:get,createElement:()=>new Element()},
        fetch:async(url,options)=>{
            if(url==='/radio/scan'){
                if(hold)await hold;
                if(failure)return {ok:false,text:async()=> 'scan unavailable'};
                return {ok:true,json:async()=>({truncated:false,networks:empty?[]:[
                    {ssid:'Open',rssi:-60,secure:false,supported:true},
                    {ssid:name,rssi:-30,secure:true,supported:true},
                    {ssid:'WPA3',rssi:-70,secure:true,supported:false}]})};
            }
            posts.push(JSON.parse(options.body));
            return {ok:true};
        }};
    vm.createContext(sandbox);vm.runInContext(source,sandbox);
    await new Promise(resolve=>setImmediate(resolve));
    assert.equal(get('s').children[1].textContent.startsWith(name),true);
    assert.equal(get('s').children[3].disabled,true);
    assert.equal(get('save').disabled,true);
    get('s').value=name;get('s').onchange();
    assert.equal(get('p').required,true);assert.equal(get('p').disabled,false);
    get('p').value='12345678';await get('f').onsubmit({preventDefault(){}});
    assert.deepEqual(posts[0],{ssid:name,password:'12345678'});assert.equal(get('p').value,'');
    get('s').value='Open';get('s').onchange();
    assert.equal(get('p').disabled,true);assert.equal(get('p').required,false);
    get('p').value='must not send';await get('f').onsubmit({preventDefault(){}});
    assert.deepEqual(posts[1],{ssid:'Open',password:''});
    let release;hold=new Promise(resolve=>{release=resolve;});
    const scanning=get('scan').onclick();assert.equal(get('scan').disabled,true);assert.equal(get('save').disabled,true);
    release();await scanning;hold=null;assert.equal(get('scan').disabled,false);
    failure=true;await get('scan').onclick();
    assert.match(get('status').textContent,/扫描失败/);assert.equal(get('save').disabled,true);assert.equal(get('scan').disabled,false);
    failure=false;empty=true;await get('scan').onclick();
    assert.match(get('status').textContent,/未找到 Wi-Fi/);assert.equal(get('save').disabled,true);
    await get('f').onsubmit({preventDefault(){}});assert.equal(posts.length,2);
    console.log('radio setup UI: selection, sorting, escaping, password, rescan and failure scenarios passed');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
