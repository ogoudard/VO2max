import SwiftUI
import CoreBluetooth


struct SettingsView: View {
    @StateObject var hrbtManager = HRBTManager()
    
    var body: some View {
        VStack {
            Button(action: {
                hrbtManager.centralManager.scanForPeripherals(withServices: [CBUUID(string: "180D")], options: nil)
            }) {
                Text("Search Heart Rate Sensors")
                    .padding()
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(8)
            }
            
            List(hrbtManager.heartRateSensors, id: \.identifier) { sensor in
                Button(action: {
                    hrbtManager.centralManager.connect(sensor, options: nil)
                }) {
                    Text(sensor.name ?? "Inconnu")
                        .padding()
                        .background(Color.gray)
                        .cornerRadius(8)
                }
            }
            
            // Affichage des données de fréquence cardiaque
            Text("Heart Rate: \(hrbtManager.heartRate)")
                .font(.title)
                .padding()
                .foregroundColor(.green)
        }
        .navigationBarTitle("Settings")
        .onAppear {
            hrbtManager.centralManager.scanForPeripherals(withServices: [CBUUID(string: "180D")], options: nil)
        }
    }
    
    
    func startSearchForHeartRateSensor() {
        print("Démarrage de la recherche pour le capteur de fréquence cardiaque...")
        // Ici tu peux appeler ta méthode de recherche Bluetooth
    }
    
    
    struct SettingsView_Previews: PreviewProvider {
        static var previews: some View {
            SettingsView()
        }
    }
    
}
