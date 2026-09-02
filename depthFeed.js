const BINANCE_WS_URL = 'wss://stream.binance.com:443/ws/btcusdt@depth20@100ms';
const BACKEND_URL = process.env.BACKEND_URL || 'http://localhost:8080';

const WebSocket = require('ws');
function connect() {
    const ws = new WebSocket(BINANCE_WS_URL);

    ws.on('open', () => console.log('Live Depth feed Connected!!'));
    ws.on('error', (err) => {
        console.error('[depthFeed] connection failed:', err.message);
    });

    setInterval(() => { if(ws.readyState === ws.OPEN) {ws.ping();}}, 30000); 
    ws.on('close', () => {
        console.error('[depthFeed] disconnected — retrying in 3s...');
        setTimeout(connect, 3000);
    });

    ws.on('message', async (data) => {
        try {
            const msg = JSON.parse(data);
            const bids = msg.bids.map(([p, q]) => ({ price: Math.round(parseFloat(p) * 100), qnty: Math.round(parseFloat(q) * 100000) }));
            const asks = msg.asks.map(([p, q]) => ({ price: Math.round(parseFloat(p) * 100), qnty: Math.round(parseFloat(q) * 100000) }));
            await fetch(`${BACKEND_URL}/admin/depth`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ bids, asks })
            });
        } catch (e) {
            console.error('Depth Feed Error', e.message);
        }
    });
}

connect();