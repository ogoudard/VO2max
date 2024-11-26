import SwiftUI
import CoreBluetooth

class HRBTManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {

    static let heartRateServiceUUID = CBUUID(string: "180D")
    static let heartRateMeasurementCharacteristicUUID = CBUUID(string: "2A37")
     
    var centralManager: CBCentralManager!
    @Published var heartRateSensors: [CBPeripheral] = []
    @Published var heartRate: String = "N/A" // Valeur par défaut avant la connexion
    private var currentPeripheral: CBPeripheral?
    
    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
    // Méthode appelée lorsque le gestionnaire Bluetooth est prêt
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            print("🟢 Bluetooth activé")
            centralManager.scanForPeripherals(withServices: [CBUUID(string: "180D")], options: nil)
        case .poweredOff:
            print("🔴 Bluetooth éteint")
        default:
            print("🔴 Bluetooth dans un état inconnu ou non disponible")
        }
    }
    
    // Méthode appelée lorsqu'un périphérique est découvert
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        guard let localName = advertisementData[CBAdvertisementDataLocalNameKey] as? String else { return }
        
        if advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID] ?? [] == [CBUUID(string: "180D")] {
            heartRateSensors.append(peripheral)
            peripheral.delegate = self
            centralManager.connect(peripheral, options: nil)
        }
    }
    
    // Méthode appelée lors de la connexion à un périphérique
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("🟢 Connecté au périphérique: \(peripheral.name ?? "Inconnu")")
        currentPeripheral = peripheral
        peripheral.discoverServices([CBUUID(string: "180D")])
    }
    
    // Méthode appelée lorsqu'un périphérique a échoué à se connecter
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        print("🔴 Échec de la connexion au périphérique: \(peripheral.name ?? "Inconnu")")
    }
    
    // Méthode appelée lors de la découverte des services
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error = error {
            print("🔴 Erreur de découverte des services: \(error.localizedDescription)")
            return
        }
        
        if let services = peripheral.services {
            for service in services {
                if service.uuid == CBUUID(string: "180D") { // Heart Rate service
                    peripheral.discoverCharacteristics([CBUUID(string: "2A37")], for: service) // Heart Rate Measurement
                }
            }
        }
    }
    
    // Méthode appelée lors de la découverte des caractéristiques d'un service
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let error = error {
            print("🔴 Erreur de découverte des caractéristiques: \(error.localizedDescription)")
            return
        }
        
        if let characteristics = service.characteristics {
            for characteristic in characteristics {
                if characteristic.uuid == CBUUID(string: "2A37") { // Heart Rate Measurement
                    peripheral.setNotifyValue(true, for: characteristic) // Activer la notification pour cette caractéristique
                }
            }
        }
    }
    
    // Méthode appelée lorsqu'une nouvelle valeur est reçue pour une caractéristique
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            print("🔴 Erreur de mise à jour de la valeur de la caractéristique: \(error.localizedDescription)")
            return
        }
        
        if characteristic.uuid == CBUUID(string: "2A37") {
            // Traitement de la valeur de la fréquence cardiaque
            if let data = characteristic.value {
                let heartRateValue = parseHeartRateData(data)
                DispatchQueue.main.async {
                    self.heartRate = "\(heartRateValue) BPM" // Mise à jour de la fréquence cardiaque
                }
            }
        }
    }
    
    // Fonction pour analyser les données de fréquence cardiaque reçues
    private func parseHeartRateData(_ data: Data) -> Int {
        let heartRate = Int(data[1]) // Le premier octet est le format de l'intervalle
        return heartRate
    }
}
