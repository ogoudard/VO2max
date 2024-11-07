
import SwiftUI
import Charts
import Foundation
import CoreBluetooth
import UniformTypeIdentifiers

struct ContentView: View {
    @ObservedObject var bluetoothManager = BluetoothManager()
    private let chartHeight: CGFloat = 200
    
    var body: some View {
        NavigationView {
            VStack {
                //Text("VO2Max Tracker")
                //    .font(.largeTitle) // Vous pouvez changer la taille de la police ici
                //    .fontWeight(.bold) // Mettre le texte en gras
                //    .padding(.top, 40) // Ajoute un espacement en haut
                // Bouton "Settings" sous le titre
                HStack {
                    Button(action: {
                        // Action à déclencher quand le bouton est appuyé
                        print("Bouton Settings appuyé")
                    }) {
                        Image(systemName: "gearshape.fill") // Icône de paramètres
                        Text("Settings")
                            .font(.headline)
                    }
                    .padding()
                    Spacer()
                    Button(action: {
                        // Action à déclencher quand le bouton est appuyé
                        print("Bouton Golden Lungs appuyé")
                    }) {
                        Image(systemName: "figure.run") // Icône de paramètres
                        Text("Poumons d'or")
                            .font(.headline)
                    }
                }
                .padding()
                
                .background(Color.blue.opacity(0.1))
                .cornerRadius(8)
                
                
                
                HStack {
                    Button(action:exportCSV) {
                        Image(systemName: "doc.text.fill") // Icône de paramètres
                        Text("Export CSV data file")
                            .font(.headline)
                    }
                }
                .padding()
                
                
                Text("VO2 Max. = \(bluetoothManager.vo2Maxvalue) mL/min/kg")
                    .font(.caption)
                    .padding(.bottom, 2)
                    .onAppear {
                        bluetoothManager.startBluetooth()
                    }
                Text("VO2 Live = \(bluetoothManager.vo2value) mL/min/kg")
                    .font(.caption)
                    .padding(.bottom, 2)
                Text("VCO2 Live = \(bluetoothManager.vco2value) mL/min")
                    .font(.caption)
                    .padding(.bottom, 2)
                Text("RQ Level = \(bluetoothManager.RQvalue) %")
                    .font(.caption)
                    .padding(.bottom, 2)
                Text("O2 = \(bluetoothManager.o2percvalue) %")
                    .font(.caption)
                    .padding(.bottom, 2)
                Text("CO2 = \(bluetoothManager.co2percvalue) %")
                    .font(.caption)
                    .padding(.bottom, 2)
                Text("Energy = \(bluetoothManager.energyvalue) kcal")
                    .font(.caption)
                    .padding(.bottom, 2)
                Text("VO2MAX evolution")
                    .font(.caption)
                    .padding(.bottom, 2)
              Chart {
                    ForEach(bluetoothManager.vo2Maxvalues, id: \.time) { entry in
                                LineMark(
                                    x: .value("Time", entry.time),
                                    y: .value("VO2Max", entry.value)
                                )
                                .foregroundStyle(Color.blue) // Couleur de la courbe
                                .lineStyle(StrokeStyle(lineWidth: 2)) // Épaisseur de la ligne
                                .interpolationMethod(.catmullRom) // Méthode d'interpolation
                            }
                        }
               // .padding(.top)
                //.frame(height: 75) // Ajuste la taille du graphique si nécessaire
                Text("VO2 evolution")
                    .font(.caption)
                    .padding(.bottom, 2)

                Chart {
                    ForEach(bluetoothManager.vo2values, id: \.time) { entry in
                                LineMark(
                                    x: .value("Time", entry.time),
                                    y: .value("VO2Max", entry.value)
                                )
                                .foregroundStyle(Color.red) // Couleur de la courbe
                                .lineStyle(StrokeStyle(lineWidth: 2)) // Épaisseur de la ligne
                                .interpolationMethod(.catmullRom) // Méthode d'interpolation
                            }
                        }

                Text("CO2 evolution")
                    .font(.caption)
                    .padding(.bottom, 2)

                //.frame(height: 75) // Ajuste la taille du graphique si nécessaire
                Chart {
                    ForEach(bluetoothManager.vco2values, id: \.time) { entry in
                                LineMark(
                                    x: .value("Time", entry.time),
                                    y: .value("VO2Max", entry.value)
                                )
                                .foregroundStyle(Color.green) // Couleur de la courbe
                                .lineStyle(StrokeStyle(lineWidth: 2)) // Épaisseur de la ligne
                                .interpolationMethod(.catmullRom) // Méthode d'interpolation
                            }
                        }

                //.frame(height: 75) // Ajuste la taille du graphique si nécessaire
 
                //.background(Color.white) // Ajoutez une couleur de fond pour mieux visualiser le graphique
                //.border(Color.gray) // Optionnel : ajoutez une bordure pour la visibilité
            }
            .navigationTitle("VO2 MAX") // Titre en haut de la fenêtre
            .navigationBarTitleDisplayMode(.inline) // Centre le titre
            
            .toolbar {
                ToolbarItem(placement: .principal) {
                    Text("VO2 MAX")
                        .font(.largeTitle) // Définissez la taille ici
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Text("Bluetooth")
                        .font(.caption)
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    // Rond indicateur de connexion
                    Circle()
                        .fill(bluetoothManager.connectionColor)
                        .frame(width: 20, height: 20)
                }
                
            }
            
        }
    }

    
    
    func exportCSV() {
        // Créer le contenu du fichier CSV avec les en-têtes de colonne
          var csvText = "Time, VO2Max, VO2, VCO2, RQ\n"
        // Obtenir la longueur minimale des listes pour éviter les erreurs d'index
        let dataCount = min(bluetoothManager.vo2Maxvalues.count,
                            bluetoothManager.vo2values.count,
                            bluetoothManager.vco2values.count,
                            bluetoothManager.RQvalues.count)
        
        // Boucle sur chaque ensemble de données pour écrire les valeurs sur une même ligne
        for index in 0..<dataCount {
            let time = bluetoothManager.vo2Maxvalues[index].time
            let vo2MaxValue = bluetoothManager.vo2Maxvalues[index].value
            let vo2Value = bluetoothManager.vo2values[index].value
            let vco2Value = bluetoothManager.vco2values[index].value
            let rqValue = bluetoothManager.RQvalues[index].value
            
            // Ajouter les données sur une ligne CSV
            csvText += "\(time), \(vo2MaxValue), \(vo2Value), \(vco2Value), \(rqValue)\n"
        }
    
     // Convertir en Data
     guard let csvData = csvText.data(using: .utf8) else { return }
     
     // Chemin temporaire pour le fichier
     let tempURL = FileManager.default.temporaryDirectory.appendingPathComponent("VO2MaxData_\(Date().formatted()).csv")
     
     // Écrire les données CSV dans le fichier temporaire
     do {
         try csvData.write(to: tempURL)
         showSaveFileDialog(fileURL: tempURL)
     } catch {
         print("Erreur lors de la création du fichier temporaire : \(error.localizedDescription)")
     }
 }
    // Fonction pour ouvrir le dialogue de sauvegarde de fichier
    func showSaveFileDialog(fileURL: URL) {
        let picker = UIDocumentPickerViewController(forExporting: [fileURL], asCopy: true)
        //picker.delegate = context.coordinator // Créez un `Coordinator` si besoin
        picker.allowsMultipleSelection = false
        UIApplication.shared.windows.first?.rootViewController?.present(picker, animated: true, completion: nil)
    }
    
    
    class BluetoothManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
        @Published var vo2Maxvalue: Float = 0 // Valeur affichée mise à jour
        @Published var vo2Maxvalues: [(time: String, value: Float)] = [
            (time: "08:00", value: 32.0),
            (time: "08:05", value: 34.5),
            (time: "08:10", value: 35.2),
            (time: "08:15", value: 37.8),
            (time: "08:20", value: 38.1),
            (time: "08:25", value: 40.4),
            (time: "08:30", value: 42.7),
            (time: "08:35", value: 45.2),
            (time: "08:40", value: 46.5),
            (time: "08:45", value: 47.9),
            (time: "08:50", value: 49.3),
            (time: "08:55", value: 50.6),
            (time: "09:00", value: 51.8) // Valeurs VO2max pour le graphique
            ]
        @Published var vo2value: Float = 0 // Valeur affichée mise à jour
        @Published var vo2values: [(time: String, value: Float)] = [
            (time: "08:00", value: 32.0),
            (time: "08:05", value: 34.5),
            (time: "08:10", value: 35.2),
            (time: "08:15", value: 37.8),
            (time: "08:20", value: 38.1),
            (time: "08:25", value: 40.4),
            (time: "08:30", value: 42.7),
            (time: "08:35", value: 45.2),
            (time: "08:40", value: 46.5),
            (time: "08:45", value: 47.9),
            (time: "08:50", value: 49.3),
            (time: "08:55", value: 50.6),
            (time: "09:00", value: 51.8) // Valeurs VO2max pour le graphique

        ] // Valeurs VO2max pour le graphique
        @Published var vco2value: Float = 0 // Valeur affichée mise à jour
        @Published var vco2values: [(time: String, value: Float)] = [
            (time: "08:00", value: 32.0),
            (time: "08:05", value: 34.5),
            (time: "08:10", value: 35.2),
            (time: "08:15", value: 37.8),
            (time: "08:20", value: 38.1),
            (time: "08:25", value: 40.4),
            (time: "08:30", value: 42.7),
            (time: "08:35", value: 45.2),
            (time: "08:40", value: 46.5),
            (time: "08:45", value: 47.9),
            (time: "08:50", value: 49.3),
            (time: "08:55", value: 50.6),
            (time: "09:00", value: 51.8) // Valeurs VO2max pour le graphique

        ] // Valeurs VO2max pour le graphique
        @Published var RQvalue: Float = 0 // Valeur affichée mise à jour
        @Published var RQvalues: [(time: String, value: Float)] = [
            (time: "08:00", value: 32.0),
            (time: "08:05", value: 34.5),
            (time: "08:10", value: 35.2),
            (time: "08:15", value: 37.8),
            (time: "08:20", value: 38.1),
            (time: "08:25", value: 40.4),
            (time: "08:30", value: 42.7),
            (time: "08:35", value: 45.2),
            (time: "08:40", value: 46.5),
            (time: "08:45", value: 47.9),
            (time: "08:50", value: 49.3),
            (time: "08:55", value: 50.6),
            (time: "09:00", value: 51.8) // Valeurs VO2max pour le graphique

        ] // Valeurs VO2max pour le graphique
        @Published var o2percvalue: Float = 0 // Valeur affichée mise à jour
        @Published var o2percvalues: [(time: String, value: Float)] = [] // Valeurs VO2max pour le graphique
        @Published var co2percvalue: Float = 0 // Valeur affichée mise à jour
        @Published var co2percvalues: [(time: String, value: Float)] = [] // Valeurs VO2max pour le graphique
        @Published var energyvalue: Float = 0 // Valeur affichée mise à jour
        @Published var energyvalues: [(time: String, value: Float)] = [] // Valeurs VO2max pour le graphique
        @Published var connectionColor: Color = .red  // Rouge par défaut (déconnecté)
        
        private var centralManager: CBCentralManager!
        private var peripheral: CBPeripheral?
        private var vo2MaxCharacteristic: CBCharacteristic?
        private var vo2Characteristic: CBCharacteristic?
        private var vco2Characteristic: CBCharacteristic?
        private var RQCharacteristic: CBCharacteristic?
        private var o2percCharacteristic: CBCharacteristic?
        private var co2percCharacteristic: CBCharacteristic?
        private var energyCharacteristic: CBCharacteristic?
        private var startTime: Date = Date() // Pour calculer le temps
        
        override init() {
            super.init()
            centralManager = CBCentralManager(delegate: self, queue: nil)
        }
        
        func startBluetooth() {
            print("🟢 Début du scan Bluetooth")
            centralManager.delegate = self
            centralManager.scanForPeripherals(withServices: [CBUUID(string: "0x180D")], options: nil)
            //centralManager.scanForPeripherals(withServices: [CBUUID(string: "4225d51b-f1c2-419a-9acc-bd8e70d960ae")], options: nil)
        }
        
        func centralManagerDidUpdateState(_ central: CBCentralManager) {
            print("🟡 État du Bluetooth: \(central.state.rawValue)")
            if central.state == .poweredOn {
                print("🟢 Bluetooth activé, début du scan")
                centralManager.scanForPeripherals(withServices: [CBUUID(string: "0x180D")], options: nil)
                //centralManager.scanForPeripherals(withServices: [CBUUID(string: "4225d51b-f1c2-419a-9acc-bd8e70d960ae")], options: nil)
            } else {
                print("🔴 Bluetooth non disponible")
            }
            switch central.state {
            case .poweredOn:
                connectionColor = .blue // En cours de connexion
            case .poweredOff, .unsupported, .unauthorized, .resetting:
                connectionColor = .red // Bluetooth indisponible
            case .unknown:
                break
            @unknown default:
                break
            }
            
            
        }
        
        func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
            print("🟢 Périphérique découvert: \(peripheral.name ?? "Inconnu")")
            self.peripheral = peripheral
            self.peripheral?.delegate = self
            centralManager.stopScan()
            print("🔵 Connexion au périphérique")
            centralManager.connect(peripheral, options: nil)
        }
        
        func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
            print("🟢 Connecté au périphérique: \(peripheral.name ?? "Inconnu")")
            peripheral.discoverServices([CBUUID(string: "0x180D")])
            peripheral.discoverServices([CBUUID(string: "231c616b-32a6-4b93-9f0c-fe728deca0a5")])
            peripheral.discoverServices([CBUUID(string: "196c1c76-fe53-47e7-baab-a32b404d63df")])
            peripheral.discoverServices([CBUUID(string: "8868ba7f-cada-4035-85d2-e0002aeb2be6")])
            peripheral.discoverServices([CBUUID(string: "f362445d-0824-40e1-a53a-8e90106f0ba3")])
            peripheral.discoverServices([CBUUID(string: "4b1ad6ec-d8d3-426c-8d1e-0dea4d6b4ebc")])
            peripheral.discoverServices([CBUUID(string: "3483923e-e94a-4a21-96c4-c12a442ee4cf")])
            connectionColor = .green // Connecté
            
        }
        
        
        
        
        func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
            if let error = error {
                print("🔴 Erreur de découverte des services: \(error)")
                return
            }
            
            print("🟢 Services découverts")
            for service in peripheral.services ?? [] {
                print("🔵 Découverte des caractéristiques pour le service \(service.uuid)")
                peripheral.discoverCharacteristics([CBUUID(string: "0x2A37")], for: service)
                peripheral.discoverCharacteristics([CBUUID(string: "4225d51b-f1c2-419a-9acc-bd8e70d960ae")], for: service)
                peripheral.discoverCharacteristics([CBUUID(string: "a4534c9e-0ede-48ec-bee5-7b728ecb25df")], for: service)
                peripheral.discoverCharacteristics([CBUUID(string: "b192eef4-a298-43fe-b85c-b04d934448f7")], for: service)
                peripheral.discoverCharacteristics([CBUUID(string: "f1329c52-97b8-4945-a8a2-2127426d71ba")], for: service)
                peripheral.discoverCharacteristics([CBUUID(string: "86c8ca0e-885f-4987-82ba-a794574cc6cf")], for: service)
                peripheral.discoverCharacteristics([CBUUID(string: "fb6fc35b-2896-4474-8647-bd206b3d0c50")], for: service)
            }
        }
        
        func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
            if let error = error {
                print("🔴 Erreur de découverte des caractéristiques: \(error)")
                return
            }
            
            for characteristic in service.characteristics ?? [] {
                if characteristic.uuid == CBUUID(string: "0x2A37") {
                    vo2MaxCharacteristic = characteristic
                    print("🔵 Enregistrement de notifications pour la caractéristique VO2Max")
                    peripheral.setNotifyValue(true, for: characteristic)
                }
                if characteristic.uuid == CBUUID(string: "4225d51b-f1c2-419a-9acc-bd8e70d960ae") {
                    vo2Characteristic = characteristic
                    print("🔵 Enregistrement de notifications pour la caractéristique VO2")
                    peripheral.setNotifyValue(true, for: characteristic)
                }
                if characteristic.uuid == CBUUID(string: "a4534c9e-0ede-48ec-bee5-7b728ecb25df") {
                    vco2Characteristic = characteristic
                    print("🔵 Enregistrement de notifications pour la caractéristique VCO2")
                    peripheral.setNotifyValue(true, for: characteristic)
                }
                if characteristic.uuid == CBUUID(string: "b192eef4-a298-43fe-b85c-b04d934448f7") {
                    RQCharacteristic = characteristic
                    print("🔵 Enregistrement de notifications pour la caractéristique RQ")
                    peripheral.setNotifyValue(true, for: characteristic)
                }
                if characteristic.uuid == CBUUID(string: "f1329c52-97b8-4945-a8a2-2127426d71ba") {
                    o2percCharacteristic = characteristic
                    print("🔵 Enregistrement de notifications pour la caractéristique O2perc")
                    peripheral.setNotifyValue(true, for: characteristic)
                }
                if characteristic.uuid == CBUUID(string: "86c8ca0e-885f-4987-82ba-a794574cc6cf") {
                    co2percCharacteristic = characteristic
                    print("🔵 Enregistrement de notifications pour la caractéristique CO2")
                    peripheral.setNotifyValue(true, for: characteristic)
                }
                if characteristic.uuid == CBUUID(string: "fb6fc35b-2896-4474-8647-bd206b3d0c50") {
                    energyCharacteristic = characteristic
                    print("🔵 Enregistrement de notifications pour la caractéristique energy")
                    peripheral.setNotifyValue(true, for: characteristic)
                }
                
            }
        }
        
        func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
            guard error == nil else {
                print("Erreur de mise à jour de la caractéristique: \(error!)")
                return
            }
            if  characteristic.uuid == CBUUID(string: "0x2A37"){
                if let data = characteristic.value {
                    print("🟢 Données reçues: \(data)")
                    vo2Maxvalue = parseData(data)
                    print("🟢 Nouvelle valeur VO2max : \(vo2Maxvalue)")
                    // Ajoutez la nouvelle valeur à la liste
                    let elapsedTime = Date().timeIntervalSince(startTime)
                    let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                    vo2Maxvalues.append((time: timeString, value: vo2Maxvalue))
                    
                }
                
            }
            if  characteristic.uuid == CBUUID(string: "4225d51b-f1c2-419a-9acc-bd8e70d960ae"){
                if let data = characteristic.value {
                    print("🟢 Données reçues: \(data)")
                    vo2value = parseDatavo2(data)
                    print("🟢 Nouvelle valeur VO2 : \(vo2value)")
                    // Ajoutez la nouvelle valeur à la liste
                    let elapsedTime = Date().timeIntervalSince(startTime)
                    let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                    vo2values.append((time: timeString, value: vo2value))
                    
                }
                
            }
            if  characteristic.uuid == CBUUID(string: "a4534c9e-0ede-48ec-bee5-7b728ecb25df"){
                if let data = characteristic.value {
                    print("🟢 Données reçues: \(data)")
                    vco2value = parseData(data)
                    print("🟢 Nouvelle valeur VCO2max : \(vco2value)")
                    // Ajoutez la nouvelle valeur à la liste
                    let elapsedTime = Date().timeIntervalSince(startTime)
                    let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                    vco2values.append((time: timeString, value: vco2value))
                    
                }
                
            }
            if  characteristic.uuid == CBUUID(string: "b192eef4-a298-43fe-b85c-b04d934448f7"){
                if let data = characteristic.value {
                    print("🟢 Données reçues: \(data)")
                    RQvalue = parseData(data)
                    print("🟢 Nouvelle valeur RQ : \(RQvalue)")
                    // Ajoutez la nouvelle valeur à la liste
                    let elapsedTime = Date().timeIntervalSince(startTime)
                    let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                    RQvalues.append((time: timeString, value: RQvalue))
                    
                }
                
            }
            if  characteristic.uuid == CBUUID(string: "f1329c52-97b8-4945-a8a2-2127426d71ba"){
                if let data = characteristic.value {
                    print("🟢 Données reçues: \(data)")
                    o2percvalue = parseData(data)
                    print("🟢 Nouvelle valeur O2% : \(o2percvalue)")
                    // Ajoutez la nouvelle valeur à la liste
                    let elapsedTime = Date().timeIntervalSince(startTime)
                    let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                    o2percvalues.append((time: timeString, value: o2percvalue))
                    
                }
                
            }
            if  characteristic.uuid == CBUUID(string: "86c8ca0e-885f-4987-82ba-a794574cc6cf"){
                if let data = characteristic.value {
                    print("🟢 Données reçues: \(data)")
                    co2percvalue = parseData(data)
                    print("🟢 Nouvelle valeur co2% : \(co2percvalue)")
                    // Ajoutez la nouvelle valeur à la liste
                    let elapsedTime = Date().timeIntervalSince(startTime)
                    let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                    co2percvalues.append((time: timeString, value: co2percvalue))
                    
                }
                
            }
            if  characteristic.uuid == CBUUID(string: "fb6fc35b-2896-4474-8647-bd206b3d0c50"){
                if let data = characteristic.value {
                    print("🟢 Données reçues: \(data)")
                    energyvalue = parseData(data)
                    print("🟢 Nouvelle valeur energy : \(energyvalue)")
                    // Ajoutez la nouvelle valeur à la liste
                    let elapsedTime = Date().timeIntervalSince(startTime)
                    let timeString = String(format: "%.0f", elapsedTime) // Format du temps en secondes
                    energyvalues.append((time: timeString, value: energyvalue))
                    
                }
                
            }
            
            
            
            
            
        }
        
        
        func parseData(_ data: Data) -> Float {
            guard data.count == 4 else {
                print("Erreur : Taille des données incorrecte")
                return 0.0
            }
            let floatValue = data.withUnsafeBytes { $0.load(as: Float.self) }
            print("Valeur VO2max reçue : \(floatValue)")
            return floatValue
        }
        
        func parseDatavo2(_ data: Data) -> Float {
            guard data.count == 4 else {
                print("Erreur : Taille des données incorrecte")
                return 0.0
            }
            let floatValue2 = data.withUnsafeBytes { $0.load(as: Float.self) }
            print("Valeur VO2 reçue : \(floatValue2)")
            return floatValue2
        }
        
        
        func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
            connectionColor = .red // Déconnecté
        }
        
        
        
        
    }
    

}

  extension Date {
      func formatted() -> String {
          let formatter = DateFormatter()
          formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
          return formatter.string(from: self)
      }
  }

