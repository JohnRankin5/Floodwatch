const POLL_INTERVAL_MS = 2500; // Poll every 2.5 seconds

async function fetchDashboardData() {
    try {
        const response = await fetch('/api/data');
        if (!response.ok) throw new Error('Network response was not ok');
        const data = await response.json();
        
        updateNodesGrid(data.nodes);
        updateDataTable(data.data);
    } catch (error) {
        console.error('Error fetching data:', error);
    }
}

function updateNodesGrid(nodes) {
    const grid = document.getElementById('nodes-grid');
    if (!nodes || nodes.length === 0) {
        grid.innerHTML = '<p style="color: #94a3b8;">No nodes detected yet...</p>';
        return;
    }
    
    grid.innerHTML = nodes.map(node => {
        const locText = node.location ? escapeHtml(node.location) : "Unknown Location";
        return `
        <div class="node-card">
            <h3>N${node.from_node}</h3>
            <p class="node-location" title="${locText}">${locText} <button class="edit-btn" onclick="editLocation(${node.from_node})">✏️</button></p>
            <p>Last Seen: ${node.last_seen.split(' ')[1]}</p>
            <div class="badge">${node.packets} pkts</div>
        </div>
        `;
    }).join('');
}

async function editLocation(nodeId) {
    const newLocation = prompt(`Enter new location/coordinate for Node ${nodeId}:`);
    if (newLocation !== null) {
        try {
            const response = await fetch(`/api/node/${nodeId}/location`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ location: newLocation })
            });
            if (response.ok) {
                fetchDashboardData(); // Rapid refresh
            }
        } catch (error) {
            console.error('Failed to update location', error);
        }
    }
}

function getSignalClass(rssi) {
    if (rssi >= -70) return 'signal-good';   // Excellent
    if (rssi >= -100) return 'signal-ok';    // Okay
    return 'signal-poor';                    // Weak
}

function updateDataTable(records) {
    const tbody = document.getElementById('data-body');
    if (!records || records.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" style="text-align:center; color: #94a3b8;">Waiting for data...</td></tr>';
        return;
    }
    
    tbody.innerHTML = records.map(record => {
        // Extract just the time part since the dashboard is for live ops
        const timeStr = record.timestamp.split(' ')[1] || record.timestamp;
        const sigClass = getSignalClass(record.rssi);
        
        return `
            <tr>
                <td>${timeStr}</td>
                <td><strong>N${record.from_node}</strong> &rarr; N${record.to_node == 255 ? 'BC' : record.to_node}</td>
                <td>#${record.seq_id}</td>
                <td class="${sigClass}">${record.rssi} dBm / ${record.snr} dB</td>
                <td><code>${escapeHtml(record.payload)}</code></td>
            </tr>
        `;
    }).join('');
}

function escapeHtml(unsafe) {
    if (!unsafe) return "";
    return unsafe
         .toString()
         .replace(/&/g, "&amp;")
         .replace(/</g, "&lt;")
         .replace(/>/g, "&gt;")
         .replace(/"/g, "&quot;")
         .replace(/'/g, "&#039;");
}

// Initial fetch and start polling loop
fetchDashboardData();
setInterval(fetchDashboardData, POLL_INTERVAL_MS);
