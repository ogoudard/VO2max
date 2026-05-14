import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

// ─────────────────────────────────────────────
// UUIDs ESP32
// ─────────────────────────────────────────────

const String CHR_VO2MAX = 'c70c73fd-b5fb-4a5f-87f4-7a187590b0ef';
const String CHR_VO2 = '4225d51b-f1c2-419a-9acc-bd8e70d960ae';
const String CHR_VCO2 = 'a4534c9e-0ede-48ec-bee5-7b728ecb25df';
const String CHR_RQ = 'b192eef4-a298-43fe-b85c-b04d934448f7';

const String SVC_VO2MAX = 'b354cf1b-2486-4f21-b4b1-ee4cd5cc3bf0';
const String SVC_VO2 = '231c616b-32a6-4b93-9f0c-fe728deca0a5';
const String SVC_VCO2 = '196c1c76-fe53-47e7-baab-a32b404d63df';
const String SVC_RQ = '8868ba7f-cada-4035-85d2-e0002aeb2be6';

// ─────────────────────────────────────────────
// SERVICE STANDARD CARDIO BLE
// Heart Rate Service
// ─────────────────────────────────────────────

const String HEART_RATE_SERVICE = '180d';
const String HEART_RATE_CHAR = '2a37';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const BleTestApp());
}

class BleTestApp extends StatelessWidget {
  const BleTestApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'VO2 BLE',
      theme: ThemeData.dark(useMaterial3: true),
      home: const HomePage(),
    );
  }
}

// ─────────────────────────────────────────────
// HOME PAGE
// ─────────────────────────────────────────────

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  final List<ScanResult> _results = [];

  StreamSubscription<List<ScanResult>>? _scanSub;

  bool _scanning = false;

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

    // attendre que le bluetooth soit ON
    if (await FlutterBluePlus.adapterState.first !=
        BluetoothAdapterState.on) {
      setState(() {
        _scanning = false;
      });
      return;
    }

    _scanSub?.cancel();

    _scanSub = FlutterBluePlus.scanResults.listen((results) {
      for (final r in results) {
        final alreadyExists = _results.any(
          (e) => e.device.remoteId == r.device.remoteId,
        );

        if (!alreadyExists) {
          setState(() {
            _results.add(r);
          });
        }
      }
    });

    await FlutterBluePlus.startScan(
      timeout: const Duration(seconds: 6),
    );

    setState(() {
      _scanning = false;
    });
  }

  void _connect(BluetoothDevice device) {
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (_) => DevicePage(device: device),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('VO2 BLE Scan'),
      ),
      body: _results.isEmpty
          ? Center(
              child: _scanning
                  ? const Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        CircularProgressIndicator(),
                        SizedBox(height: 16),
                        Text('Scan BLE en cours...'),
                      ],
                    )
                  : const Text('Appuie sur Scan'),
            )
          : ListView.builder(
              itemCount: _results.length,
              itemBuilder: (_, index) {
                final r = _results[index];

                final deviceName = r.device.name.isEmpty
                    ? '(sans nom)'
                    : r.device.name;

                final isVO2 = deviceName == 'VO2-HR';

                final isPolar =
                    deviceName.toUpperCase().contains('POLAR');

                return ListTile(
                  leading: Icon(
                    Icons.bluetooth,
                    color: isVO2 || isPolar
                        ? Colors.greenAccent
                        : Colors.grey,
                  ),
                  title: Text(
                    deviceName,
                    style: TextStyle(
                      fontWeight:
                          isVO2 || isPolar
                              ? FontWeight.bold
                              : FontWeight.normal,
                    ),
                  ),
                  subtitle: Text(
                    r.device.remoteId.toString(),
                  ),
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
                child: CircularProgressIndicator(
                  strokeWidth: 2,
                ),
              )
            : const Icon(Icons.search),
        label: Text(_scanning ? 'Scan...' : 'Scan'),
      ),
    );
  }
}

// ─────────────────────────────────────────────
// DEVICE PAGE
// ─────────────────────────────────────────────

class DevicePage extends StatefulWidget {
  final BluetoothDevice device;

  const DevicePage({
    super.key,
    required this.device,
  });

  @override
  State<DevicePage> createState() => _DevicePageState();
}

class _DevicePageState extends State<DevicePage> {
  bool _connecting = false;
  bool _connected = false;

  String _log = '';

  double? vo2max;
  double? vo2;
  double? vco2;
  double? rq;

  int? heartRate;

  final List<StreamSubscription> _subscriptions = [];

  @override
  void dispose() {
    for (final s in _subscriptions) {
      s.cancel();
    }

    widget.device.disconnect();

    super.dispose();
  }

  void _addLog(String msg) {
    setState(() {
      _log = '$msg\n$_log';
    });
  }

  ({String chrId, String label})? _serviceMapping(
    String svcId,
  ) {
    final id = svcId.toLowerCase();

    if (id == SVC_VO2MAX.toLowerCase()) {
      return (
        chrId: CHR_VO2MAX,
        label: 'VO2Max',
      );
    }

    if (id == SVC_VO2.toLowerCase()) {
      return (
        chrId: CHR_VO2,
        label: 'VO2',
      );
    }

    if (id == SVC_VCO2.toLowerCase()) {
      return (
        chrId: CHR_VCO2,
        label: 'VCO2',
      );
    }

    if (id == SVC_RQ.toLowerCase()) {
      return (
        chrId: CHR_RQ,
        label: 'RQ',
      );
    }

    return null;
  }

  void _handleValue(
    List<int> value,
    String label,
  ) {
    if (value.length < 4) return;

    final data = ByteData.sublistView(
      Uint8List.fromList(value),
    );

    final f = data.getFloat32(
      0,
      Endian.little,
    );

    setState(() {
      switch (label) {
        case 'VO2Max':
          vo2max = f;
          break;

        case 'VO2':
          vo2 = f;
          break;

        case 'VCO2':
          vco2 = f;
          break;

        case 'RQ':
          rq = f;
          break;
      }
    });
  }

  // ─────────────────────────────────────────────
  // HEART RATE BLE STANDARD
  // ─────────────────────────────────────────────

  void _handleHeartRate(List<int> value) {
    if (value.isEmpty) return;

    int hr = 0;

    final flags = value[0];

    final hr16Bits = (flags & 0x01) != 0;

    if (hr16Bits) {
      hr = value[1] | (value[2] << 8);
    } else {
      hr = value[1];
    }

    setState(() {
      heartRate = hr;
    });
  }

  Future<void> _connectAndSubscribe() async {
    setState(() {
      _connecting = true;
    });

    try {
      await widget.device.connect(
        autoConnect: false,
      );

      setState(() {
        _connected = true;
      });

      _addLog('✅ Connecté');

      final services =
          await widget.device.discoverServices();

      _addLog(
        '🔍 ${services.length} service(s) trouvé(s)',
      );

      // ─────────────────────────────────────────
      // AFFICHAGE DES SERVICES
      // ─────────────────────────────────────────

      for (final s in services) {
        _addLog(
          'SERVICE => ${s.serviceUuid}',
        );

        for (final c in s.characteristics) {
          _addLog(
            'CHAR => ${c.characteristicUuid}',
          );
        }
      }

      // ─────────────────────────────────────────
      // SERVICES ESP32
      // ─────────────────────────────────────────

      for (final service in services) {
        final svcId =
            service.serviceUuid.toString();

        final mapping = _serviceMapping(svcId);

        if (mapping == null) {
          continue;
        }

        BluetoothCharacteristic? characteristic;

        for (final c in service.characteristics) {
          if (c.characteristicUuid
                  .toString()
                  .toLowerCase() ==
              mapping.chrId.toLowerCase()) {
            characteristic = c;
            break;
          }
        }

        if (characteristic == null) {
          _addLog(
            '⚠️ Caractéristique introuvable ${mapping.label}',
          );
          continue;
        }

        await characteristic.setNotifyValue(true);

        _addLog(
          '🔔 Notification activée ${mapping.label}',
        );

        final sub =
            characteristic.onValueReceived.listen(
          (value) {
            _handleValue(
              value,
              mapping.label,
            );
          },
        );

        _subscriptions.add(sub);

        if (characteristic.properties.read) {
          final initial =
              await characteristic.read();

          _handleValue(
            initial,
            mapping.label,
          );
        }
      }

      // ─────────────────────────────────────────
      // HEART RATE STANDARD BLE
      // ─────────────────────────────────────────

      for (final service in services) {
        final svcId =
            service.serviceUuid
                .toString()
                .toLowerCase();

        if (!svcId.contains(HEART_RATE_SERVICE)) {
          continue;
        }

        _addLog('❤️ Service cardio trouvé');

        for (final c in service.characteristics) {
          final chrId =
              c.characteristicUuid
                  .toString()
                  .toLowerCase();

          if (!chrId.contains(HEART_RATE_CHAR)) {
            continue;
          }

          _addLog('❤️ Characteristic HR trouvée');

          await c.setNotifyValue(true);

          final sub = c.onValueReceived.listen(
            (value) {
              _handleHeartRate(value);
            },
          );

          _subscriptions.add(sub);
        }
      }
    } catch (e) {
      _addLog('❌ Erreur : $e');
    }

    setState(() {
      _connecting = false;
    });
  }

  Widget _valueCard(
    String label,
    String value,
    String unit,
    Color color,
  ) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(
          horizontal: 24,
          vertical: 16,
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(
              label,
              style: const TextStyle(
                fontSize: 13,
                color: Colors.grey,
              ),
            ),
            const SizedBox(height: 6),
            Text(
              value,
              style: TextStyle(
                fontSize: 28,
                fontWeight: FontWeight.bold,
                color: color,
              ),
            ),
            Text(
              unit,
              style: const TextStyle(
                fontSize: 11,
                color: Colors.grey,
              ),
            ),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final deviceName =
        widget.device.name.isEmpty
            ? widget.device.remoteId.toString()
            : widget.device.name;

    return Scaffold(
      appBar: AppBar(
        title: Text(deviceName),
        actions: [
          if (_connected)
            const Padding(
              padding: EdgeInsets.only(right: 12),
              child: Icon(
                Icons.circle,
                color: Colors.greenAccent,
                size: 14,
              ),
            ),
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
                _valueCard(
                  'VO2 Max',
                  vo2max != null
                      ? vo2max!.toStringAsFixed(2)
                      : '--',
                  'mL/min/kg',
                  vo2max != null
                      ? Colors.greenAccent
                      : Colors.grey,
                ),
                _valueCard(
                  'VO2',
                  vo2 != null
                      ? vo2!.toStringAsFixed(2)
                      : '--',
                  'mL/min/kg',
                  vo2 != null
                      ? Colors.greenAccent
                      : Colors.grey,
                ),
                _valueCard(
                  'VCO2',
                  vco2 != null
                      ? vco2!.toStringAsFixed(2)
                      : '--',
                  'mL/min/kg',
                  vco2 != null
                      ? Colors.greenAccent
                      : Colors.grey,
                ),
                _valueCard(
                  'RQ',
                  rq != null
                      ? rq!.toStringAsFixed(2)
                      : '--',
                  'ratio',
                  rq != null
                      ? Colors.greenAccent
                      : Colors.grey,
                ),
                _valueCard(
                  'Heart Rate',
                  heartRate != null
                      ? '$heartRate'
                      : '--',
                  'BPM',
                  heartRate != null
                      ? Colors.redAccent
                      : Colors.grey,
                ),
              ],
            ),
            const SizedBox(height: 16),
            if (!_connected)
              ElevatedButton.icon(
                onPressed:
                    _connecting
                        ? null
                        : _connectAndSubscribe,
                icon: _connecting
                    ? const SizedBox(
                        width: 16,
                        height: 16,
                        child:
                            CircularProgressIndicator(
                          strokeWidth: 2,
                        ),
                      )
                    : const Icon(Icons.link),
                label: Text(
                  _connecting
                      ? 'Connexion...'
                      : 'Connecter',
                ),
              ),
            const SizedBox(height: 16),
            Expanded(
              child: Container(
                width: double.infinity,
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: Colors.black45,
                  borderRadius:
                      BorderRadius.circular(8),
                ),
                child: SingleChildScrollView(
                  reverse: true,
                  child: Text(
                    _log.isEmpty
                        ? 'Log vide'
                        : _log,
                    style: const TextStyle(
                      fontFamily: 'monospace',
                      fontSize: 11,
                    ),
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
