'use strict';

const express = require('express');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');
const multer = require('multer');

const app = express();

// Parse JSON bodies
app.use(express.json({ limit: '10mb' }));

// Static frontend
app.use(express.static(path.join(__dirname, 'public')));

// Path to compiled binary
const BINARY = path.resolve(__dirname, '..', 'disk_manager');

// Upload directory for -cont files
const UPLOAD_DIR = path.join(os.tmpdir(), 'mia_cont');
if (!fs.existsSync(UPLOAD_DIR)) {
    fs.mkdirSync(UPLOAD_DIR, { recursive: true });
}

const upload = multer({ dest: UPLOAD_DIR });

// ─────────────────────────────────────────────
// Run commands through the binary
// ─────────────────────────────────────────────
function runCommands(commands) {
    return new Promise((resolve) => {
        const proc = spawn(BINARY, [], {
            stdio: ['pipe', 'pipe', 'pipe'],
            cwd: path.dirname(BINARY),
        });

        let output = '';
        proc.stdout.on('data', (d) => { output += d.toString(); });
        proc.stderr.on('data', (d) => { output += d.toString(); });
        proc.on('close', () => resolve(output));
        proc.on('error', (e) => resolve('Error al iniciar el binario: ' + e.message));

        const payload = commands.trim() + '\nexit\n';
        proc.stdin.write(payload);
        proc.stdin.end();
    });
}

// ─────────────────────────────────────────────
// POST /api/run  — execute commands
// ─────────────────────────────────────────────
app.post('/api/run', async (req, res) => {
    const { commands } = req.body || {};
    if (!commands || !commands.trim()) {
        return res.json({ output: '', images: [] });
    }
    try {
        const output = await runCommands(commands);
        res.json({ output });
    } catch (e) {
        res.status(500).json({ output: 'Error interno: ' + e.message });
    }
});

// ─────────────────────────────────────────────
// POST /api/upload  — upload a file for -cont=
// ─────────────────────────────────────────────
app.post('/api/upload', upload.single('file'), (req, res) => {
    if (!req.file) return res.status(400).json({ error: 'No file uploaded' });
    const dest = path.join(UPLOAD_DIR, req.file.originalname);
    fs.renameSync(req.file.path, dest);
    res.json({ path: dest });
});

// ─────────────────────────────────────────────
// GET /api/image?path=  — serve any image by absolute path
// ─────────────────────────────────────────────
app.get('/api/image', (req, res) => {
    const p = req.query.path;
    if (!p || !fs.existsSync(p)) return res.status(404).send('Not found');
    res.sendFile(p);
});

// ─────────────────────────────────────────────
// GET /api/textfile?path=  — serve a text file
// ─────────────────────────────────────────────
app.get('/api/textfile', (req, res) => {
    const p = req.query.path;
    if (!p || !fs.existsSync(p)) return res.status(404).send('Not found');
    res.setHeader('Content-Type', 'text/plain; charset=utf-8');
    res.sendFile(p);
});

// ─────────────────────────────────────────────
// Start
// ─────────────────────────────────────────────
const PORT = process.env.PORT || 8080;
app.listen(PORT, () => {
    console.log(`MIA Frontend → http://localhost:${PORT}`);
    console.log(`Binary: ${BINARY}`);
});
