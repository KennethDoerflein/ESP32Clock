// OtaManager.h

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <ESPAsyncWebServer.h>

// Define a type for the template processor function
using TplProcessor = std::function<String(const String &)>;

/**
 * @class OtaManager
 * @brief Manages firmware updates (OTA).
 *
 * This singleton class handles both web-based file uploads and fetching
 * updates from a GitHub repository. It encapsulates all logic related to
 * over-the-air updates to keep the main web server code cleaner.
 */
class OtaManager
{
public:
  /**
   * @brief Returns the singleton instance of the OtaManager.
   * @return A reference to the singleton OtaManager instance.
   */
  static OtaManager &getInstance();

  /**
   * @brief Registers the OTA update routes with the web server.
   * @param server A reference to the AsyncWebServer instance.
   * @param processor A function to process template placeholders in HTML.
   */
  void begin(AsyncWebServer &server, TplProcessor processor);

  /**
   * @brief Checks if an update is currently in progress.
   * @return True if an update is in progress, false otherwise.
   */
  bool isUpdating() const;

  // Delete copy constructor and assignment operator.
  OtaManager(const OtaManager &) = delete;
  void operator=(const OtaManager &) = delete;

private:
  // Private constructor for singleton pattern.
  OtaManager();

  /// Flag to prevent concurrent updates.
  bool _isUpdating;
  /// Stored template processor function.
  TplProcessor _processor;

  // --- Request Handlers ---
  void onUpdateRequest(AsyncWebServerRequest *request);
  void onUpdateUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);
  void onGithubUpdate(AsyncWebServerRequest *request);
};

#endif // OTA_MANAGER_H