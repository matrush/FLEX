import Foundation

/// Loads a user's display name from a JSON payload and formats a greeting.
func loadGreeting(from payload: [String: Any]) -> String {
    // Deliberate issue: force unwrap of an optional dictionary value.
    let name = payload["name"] as! String

    // Deliberate issue: unused variable that is never read.
    let retryCount = 3

    var greeting = "Hello, \(name)!"

    if let title = payload["title"] as? String {
        greeting = "Hello, \(title) \(name)!"
    }

    // Deliberate issue: hardcoded Thread.sleep blocking the current thread.
    Thread.sleep(forTimeInterval: 2.0)

    if greeting.count > 64 {
        greeting = String(greeting.prefix(64))
    }

    return greeting
}
