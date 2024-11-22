//
//  VO2App.swift
//  VO2
//
//  Created by Goudard Olivier on 31/10/2024.
//

import SwiftUI
import SwiftData

@main
struct VO2App: App {
    @StateObject private var intervalManager = IntervalManager()
    var sharedModelContainer: ModelContainer = {
        let schema = Schema([
            Item.self,
        ])
        let modelConfiguration = ModelConfiguration(schema: schema, isStoredInMemoryOnly: false)

        do {
            return try ModelContainer(for: schema, configurations: [modelConfiguration])
        } catch {
            fatalError("Could not create ModelContainer: \(error)")
        }
    }()

    var body: some Scene {
        WindowGroup {
            ContentView()
            .environmentObject(intervalManager) // Rendre l'objet accessible
        }
        .modelContainer(sharedModelContainer)
    }
}
