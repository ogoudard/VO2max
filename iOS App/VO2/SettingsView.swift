import SwiftUI
import CoreBluetooth

struct SettingsView: View {
    @ObservedObject var bluetoothManager: BluetoothManager // Ajouter ceci pour observer bluetoothManager
    @StateObject var hrbtManager = HRBTManager()
    @State private var isVO2HRConnected: Bool? = nil // État de la connexion : nil = inconnu, true = connecté, false = non connecté
    var body: some View {
        VStack {
            // Afficher la version et le build
            VStack(alignment: .leading, spacing: 8) {
                Text("App Version: \(AppInfo.version)")
                    .font(.headline)
                Text("Build Number: \(AppInfo.build)")
                    .font(.subheadline)
                    .foregroundColor(.gray)
            }
            .padding()
            
            Spacer()
            
            // Bouton pour rechercher des capteurs de fréquence cardiaque
            Button(action: {
                hrbtManager.centralManager.scanForPeripherals(withServices: [CBUUID(string: "180D")], options: nil)
            }) {
                Text("Search Heart Rate Sensors")
                    .padding()
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(8)
            }
            .padding(.bottom, 8) // Espacement en bas du bouton
            
            // Affichage des données de fréquence cardiaque juste sous le bouton
            Text("Heart Rate: \(hrbtManager.heartRate)")
                .font(.headline)
                .padding(.bottom, 16)
                .foregroundColor(.white)
            
 
            // Bouton pour vérifier si VO2-HR est connecté
            Button(action: {
                checkVO2HRConnection()
            }) {
                Text("Check VO2-HR Connection")
                    .padding()
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(8)
            }
            .padding()
            // Affichage de l'état de connexion de VO2-HR
            if let isConnected = isVO2HRConnected {
                Text(isConnected ? "VO2-HR is Connected" : "VO2-HR is Not Connected")
                    .font(.headline)
                    .foregroundColor(isConnected ? .green : .red)
            } else {
                Text("Connection status: Unknown")
                    .font(.headline)
                    .foregroundColor(.gray)
            }
            
            // Liste des capteurs détectés
            List(hrbtManager.heartRateSensors, id: \.identifier) { sensor in
                Button(action: {
                    hrbtManager.centralManager.connect(sensor, options: nil)
                }) {
                    Text(sensor.name ?? "Inconnu")
                        .background(Color.gray)
                        .padding()
                        .cornerRadius(8)
                }
            }

        }
        .navigationBarTitle("Settings")
        .onAppear {
            hrbtManager.centralManager.scanForPeripherals(withServices: [CBUUID(string: "180D")], options: nil)
        }
    }
    
    // Méthode pour vérifier la connexion VO2-HR
     private func checkVO2HRConnection() {
         // Utiliser bluetoothManager pour récupérer les périphériques connectés
         let connectedPeripherals = bluetoothManager.centralManager.retrieveConnectedPeripherals(withServices: [CBUUID(string: "b354cf1b-2486-4f21-b4b1-ee4cd5cc3bf0")])

         // Vérifiez si l'un des périphériques a le nom "VO2-HR"
         if connectedPeripherals.contains(where: { $0.name == "VO2-HR" }) {
             isVO2HRConnected = true // Le périphérique VO2-HR est connecté
         } else {
             isVO2HRConnected = false // Le périphérique VO2-HR n'est pas connecté
         }
     }
 }
struct SettingsView_Previews: PreviewProvider {
        static var previews: some View {
            SettingsView(bluetoothManager: BluetoothManager())
        }
    }

