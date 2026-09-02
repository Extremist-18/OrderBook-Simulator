const WebSocket = require('ws');
const ws = new WebSocket('wss://stream.binance.com:9443/ws/btcusdt@depth20@100ms');

ws.on('message', async (data)=>{
    try{
        const msg= JSON.parse(data);
        const bids =msg.bids.map(([p,q])=>({price:Math.round(parseFloat(p)*100), qnty:Math.round(parseFloat(q)*100000)}));
        const asks =msg.asks.map(([p,q])=>({price:Math.round(parseFloat(p)*100), qnty:Math.round(parseFloat(q)*100000)}));
        await fetch('http://localhost:8080/admin/depth',{
            method:'POST',
            headers: {'Content-Type':'application/json'},
            body :JSON.stringify({bids,asks})
        });
    }catch(e){
        console.error('Depth Feed Error',e.message);
    }
});

ws.on('open',()=>console.log('Live Depth feed Connected!!'));
