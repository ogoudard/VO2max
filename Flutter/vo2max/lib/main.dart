import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:win_ble/win_ble.dart';
import 'package:win_ble/win_file.dart';

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

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await WinBle.initialize(serverPath: await WinServer.path(), enableLog: false);
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
  final List<BleDevice> _devices = [];
  bool _scanning = false;
  StreamSubscription? _scanSub;

  @override
  void dispose() {
    _scanSub?.cancel();
    WinBle.dispose();
    super.dispose();
  }

  Future<void> _startScan() async {
    setState(() {
      _devices.clear();
      _scanning = true;
    });

    _scanSub = WinBle.scanStream.listen((device) {
      setState(() {
        if (!_devices.any((d) => d.address == device.address)) {
          _devices.add(device);
        }
      });
    });

    WinBle.startScanning();

    await Future.delayed(const Duration(seconds: 6));
    WinBle.stopScanning();
    _scanSub?.cancel();
    setState(() => _scanning = false);
  }

  void _connect(BleDevice device) {
    Navigator.push(
      context,
      MaterialPageRoute(builder: (_) => DevicePage(device: device)),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('VO2 BLE — Scan')),
      body: _devices.isEmpty
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
              itemCount: _devices.length,
              itemBuilder: (_, i) {
                final d = _devices[i];
                final name = d.name.isEmpty ? '(sans nom)' : d.name;
                final isTarget = d.name == 'VO2-HR';
                return ListTile(
                  leading: Icon(Icons.bluetooth,
                      color: isTarget ? Colors.greenAccent : Colors.grey),
                  title: Text(name,
                      style: TextStyle(
                          fontWeight: isTarget
                              ? FontWeight.bold
                              : FontWeight.normal)),
                  subtitle: Text(d.address),
                  trailing: Text('${d.rssi} dBm'),
                  onTap: () => _connect(d),
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
  final BleDevice device;
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
    WinBle.disconnect(widget.device.address);
    super.dispose();
  }

  void _addLog(String msg) =>
      setState(() => _log = '$msg\n$_log');

  Future<void> _connectAndSubscribe() async {
    setState(() => _connecting = true);
    try {
      await WinBle.connect(widget.device.address);
      _addLog('✅ Connecté');
      setState(() => _connected = true);

      // Découverte des services
      final services = await WinBle.discoverServices(widget.device.address);
      _addLog('🔍 ${services.length} service(s) trouvé(s)');

      for (final svc in services) {
        final svcId = svc.toLowerCase();

        // Caractéristiques à souscrire selon le service
        String? chrId;
        String? label;

        if (svcId == SVC_VO2MAX.toLowerCase()) {
          chrId = CHR_VO2MAX; label = 'VO2Max';
        } else if (svcId == SVC_VO2.toLowerCase()) {
          chrId = CHR_VO2; label = 'VO2';
        } else if (svcId == SVC_VCO2.toLowerCase()) {
          chrId = CHR_VCO2; label = 'VCO2';
        } else if (svcId == SVC_RQ.toLowerCase()) {
          chrId = CHR_RQ; label = 'RQ';
        }

        if (chrId == null) continue;

        await WinBle.subscribeToCharacteristic(
          address: widget.device.address,
          serviceId: svc,
          characteristicId: chrId,
        );
        _addLog('🔔 Notif activée : $label');

        final sub = WinBle.characteristicValueStream.listen((data) {
          if (data.address != widget.device.address) return;
          if (data.characteristicId.toLowerCase() != chrId!.toLowerCase()) return;
          if (data.value.length >= 4) {
            final f = ByteData.sublistView(Uint8List.fromList(data.value))
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
        });
        _subs.add(sub);
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
        title: Text(widget.device.name.isEmpty
            ? widget.device.address
            : widget.device.name),
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
                _valueCard('VO2',     vo2,    'mL/min'),
                _valueCard('VCO2',    vco2,   'mL/min'),
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
