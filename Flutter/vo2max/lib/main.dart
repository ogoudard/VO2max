import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

// ─────────────────────────────────────────────
//  UUIDs — correspondent au firmware ESP32
// ─────────────────────────────────────────────
const String CHR_VO2MAX = 'c70c73fd-b5fb-4a5f-87f4-7a187590b0ef';
const String CHR_VO2    = '4225d51b-f1c2-419a-9acc-bd8e70d960ae';
const String CHR_VCO2   = 'a4534c9e-0ede-48ec-bee5-7b728ecb25df';
const String CHR_RQ     = 'b192eef4-a298-43fe-b85c-b04d934448f7';

const String SVC_VO2MAX = 'b354cf1b-2486-4f21-b4b1-ee4cd5cc3bf0';
const String SVC_VO2    = '231c616b-32a6-4b93-9f0c-fe728deca0a5';
const String SVC_VCO2   = '196c1c76-fe53-47e7-baab-a32b404d63df';
const String SVC_RQ     = '8868ba7f-cada-4035-85d2-e0002aeb2be6';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const BleTestApp());
}

class BleTestApp extends StatelessWidget {
  const BleTestApp({super.key});
  @override
  Widget build(BuildContext context) => MaterialApp(
        title: 'VO2 BLE Test',
        theme: ThemeData.dark(useMaterial3: true),
        home: const HomePage(),
      );
}

// ─────────────────────────────────────────────
//  Home — scan
// ─────────────────────────────────────────────
class HomePage extends StatefulWidget {
  const HomePage({super.key});
  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  final List<ScanResult> _results = [];
  bool _scanning = false;
  StreamSubscription? _scanSub;

  @override
  void dispose() {
    _scanSub?.cancel();
    FlutterBluePlus.stopScan();
    super.dispose();
  }

  Future<void> _startScan() async {
    setState(() {
      _results.clear();
      _scanning = true;
    });

    _scanSub = FlutterBluePlus.scanResults.listen((results) {
      setState(() {
        for (final r in results) {
          final exists = _results.any(
              (e) => e.device.remoteId == r.device.remoteId);
          if (!exists) _results.add(r);
        }
      });
    });

    await FlutterBluePlus.startScan(timeout: const Duration(seconds: 6));

    _scanSub?.cancel();
    setState(() => _scanning = false);
  }

  void _connect(BluetoothDevice device) {
    Navigator.push(
      context,
      MaterialPageRoute(builder: (_) => DevicePage(device: device)),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('VO2 BLE — Scan')),
      body: _results.isEmpty
          ? Center(
              child: _scanning
                  ? const Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        CircularProgressIndicator(),
                        SizedBox(height: 12),
                        Text('Scan en cours...'),
                      ],
                    )
                  : const Text('Appuie sur Scan pour chercher VO2-HR'),
            )
          : ListView.builder(
              itemCount: _results.length,
              itemBuilder: (_, i) {
                final r = _results[i];
                final name = r.device.platformName.isEmpty
                    ? '(sans nom)'
                    : r.device.platformName;
                final isTarget = r.device.platformName == 'VO2-HR';
                return ListTile(
                  leading: Icon(Icons.bluetooth,
                      color: isTarget ? Colors.greenAccent : Colors.grey),
                  title: Text(name,
                      style: TextStyle(
                          fontWeight: isTarget
                              ? FontWeight.bold
                              : FontWeight.normal)),
                  subtitle: Text(r.device.remoteId.toString()),
                  trailing: Text('${r.rssi} dBm'),
                  onTap: () => _connect(r.device),
                );
              },
            ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: _scanning ? null : _startScan,
        icon: _scanning
            ? const SizedBox(
                width: 18,
                height: 18,
                child: CircularProgressIndicator(strokeWidth: 2))
            : const Icon(Icons.search),
        label: Text(_scanning ? 'Scan...' : 'Scan'),
      ),
    );
  }
}

// ─────────────────────────────────────────────
//  Device page — connexion + notifications
// ─────────────────────────────────────────────
class DevicePage extends StatefulWidget {
  final BluetoothDevice device;
  const DevicePage({super.key, required this.device});
  @override
  State<DevicePage> createState() => _DevicePageState();
}

class _DevicePageState extends State<DevicePage> {
  bool _connecting = false;
  bool _connected = false;
  String _log = '';

  double? vo2max, vo2, vco2, rq;

  final List<StreamSubscription> _subs = [];

  @override
  void dispose() {
    for (final s in _subs) s.cancel();
    widget.device.disconnect();
    super.dispose();
  }

  void _addLog(String msg) => setState(() => _log = '$msg\n$_log');

  ({String chrId, String label})? _serviceMapping(String svcId) {
    final id = svcId.toLowerCase();
    if (id == SVC_VO2MAX.toLowerCase()) return (chrId: CHR_VO2MAX, label: 'VO2Max');
    if (id == SVC_VO2.toLowerCase())    return (chrId: CHR_VO2,    label: 'VO2');
    if (id == SVC_VCO2.toLowerCase())   return (chrId: CHR_VCO2,   label: 'VCO2');
    if (id == SVC_RQ.toLowerCase())     return (chrId: CHR_RQ,     label: 'RQ');
    return null;
  }

  void _handleValue(List<int> value, String label) {
    if (value.length >= 4) {
      final f = ByteData.sublistView(Uint8List.fromList(value))
          .getFloat32(0, Endian.little);
      setState(() {
        switch (label) {
          case 'VO2Max': vo2max = f;
          case 'VO2':    vo2    = f;
          case 'VCO2':   vco2   = f;
          case 'RQ':     rq     = f;
        }
      });
    }
  }

  Future<void> _connectAndSubscribe() async {
    setState(() => _connecting = true);
    try {
      await widget.device.connect(autoConnect: false);
      _addLog('✅ Connecté');
      setState(() => _connected = true);

      final services = await widget.device.discoverServices();
      _addLog('🔍 ${services.length} service(s) trouvé(s)');

      for (final service in services) {
        final svcId = service.serviceUuid.toString();
        final mapping = _serviceMapping(svcId);
        if (mapping == null) continue;

        final chr = service.characteristics.where((c) =>
            c.characteristicUuid.toString().toLowerCase() ==
            mapping.chrId.toLowerCase()).firstOrNull;

        if (chr == null) {
          _addLog('⚠️ Caractéristique introuvable : ${mapping.label}');
          continue;
        }

        await chr.setNotifyValue(true);
        _addLog('🔔 Notif activée : ${mapping.label}');

        final sub = chr.onValueReceived.listen((value) {
          _handleValue(value, mapping.label);
        });
        _subs.add(sub);

        if (chr.properties.read) {
          final initial = await chr.read();
          _handleValue(initial, mapping.label);
        }
      }
    } catch (e) {
      _addLog('❌ Erreur : $e');
    } finally {
      setState(() => _connecting = false);
    }
  }

  Widget _valueCard(String label, double? value, String unit) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 16),
        child: Column(
          children: [
            Text(label,
                style: const TextStyle(fontSize: 13, color: Colors.grey)),
            const SizedBox(height: 4),
            Text(
              value != null ? value.toStringAsFixed(2) : '—',
              style: TextStyle(
                  fontSize: 28,
                  fontWeight: FontWeight.bold,
                  color: value != null ? Colors.greenAccent : Colors.grey),
            ),
            Text(unit,
                style: const TextStyle(fontSize: 11, color: Colors.grey)),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.device.platformName.isEmpty
            ? widget.device.remoteId.toString()
            : widget.device.platformName),
        actions: [
          if (_connected)
            const Padding(
              padding: EdgeInsets.only(right: 12),
              child: Icon(Icons.circle, color: Colors.greenAccent, size: 14),
            )
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            Wrap(
              spacing: 8,
              runSpacing: 8,
              alignment: WrapAlignment.center,
              children: [
                _valueCard('VO2 Max', vo2max, 'mL/min/kg'),
                _valueCard('VO2',     vo2,    'mL/min/kg'),
                _valueCard('VCO2',    vco2,   'mL/min/kg'),
                _valueCard('RQ',      rq,     'ratio'),
              ],
            ),
            const SizedBox(height: 16),
            if (!_connected)
              ElevatedButton.icon(
                onPressed: _connecting ? null : _connectAndSubscribe,
                icon: _connecting
                    ? const SizedBox(
                        width: 16,
                        height: 16,
                        child: CircularProgressIndicator(strokeWidth: 2))
                    : const Icon(Icons.link),
                label: Text(
                    _connecting ? 'Connexion...' : 'Connecter & Subscribe'),
              ),
            const SizedBox(height: 12),
            Expanded(
              child: Container(
                padding: const EdgeInsets.all(10),
                decoration: BoxDecoration(
                  color: Colors.black45,
                  borderRadius: BorderRadius.circular(8),
                ),
                child: SingleChildScrollView(
                  reverse: true,
                  child: Text(
                    _log.isEmpty ? 'Log vide' : _log,
                    style: const TextStyle(
                        fontFamily: 'monospace', fontSize: 11),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}