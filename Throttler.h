#pragma once
#include <folly/Synchronized.h>
#include <thread>
#include <glog/logging.h>
#include <folly/SpinLock.h>
namespace facebook {
namespace wdt {
typedef std::chrono::high_resolution_clock Clock;
/**
 * Attempts to limit the rate in two ways.
 * 1. Limit average rate by calling averageThrottler()
 * 2. Limit the peak rate by calling limitByTokenBucket
 * Generally average throttler would be mainting the rate to avgRate_
 * although at times the actual rate might fall behind and in those
 * circumstances the rate at which you will catch up is limited
 * with respect to the peak rate and the bucket limit using the
 * token bucket algorithm.
 * Token Bucket algorithm can be found on
 * http://en.wikipedia.org/wiki/Token_bucket
 */
class Throttler {
 public:
  /**
   * Utility method that configures the avg rate, peak rate and bucket limit
   * based on the values passed to this method and returns a shared ptr
   * to an instance of this throttler
   */
  static std::shared_ptr<Throttler> makeThrottler(
      double avgRateBytesPerSec, double peakRateBytesPerSec,
      double bucketLimitBytes, int64_t throttlerLogTimeMillis);

  /**
   * Sometimes the options passed to throttler might not make sense so this
   * method tries to auto configure them
   */
  static void configureOptions(double& avgRateBytesPerSec,
                               double& peakRateBytesPerSec,
                               double& bucketLimitBytes);

  /**
   * @param averageRateBytesPerSec    Average rate in progress/second
   *                                  at which data should be transmitted
   *
   * @param peakRateBytesPerSec       Max burst rate allowed by the
   *                                  token bucket
   * @param bucketLimitBytes          Max size of bucket, specify 0 for auto
   *                                  configure. In auto mode, it will be twice
   *                                  the data you send in 1/4th of a second
   *                                  at the peak rate
   */
  Throttler(double avgRateBytesPerSec, double peakRateBytesPerSec,
            double bucketLimitBytes, int64_t throttlerLogTimeMillis = 0);

  /**
   * Calls calculateSleep which is a thread safe method. Finds out the
   * time thread has to sleep and makes it sleep.
   * Also calls the throttler logger to log the stats
   */
  virtual void limit(double deltaProgress);

  /**
   * This is thread safe implementation of token bucket
   * algorithm. Bucket is filled at the rate of bucketRateBytesPerSec_
   * till the limit of bytesTokenBucketLimit_
   * There is no sleep, we just calcaulte how much to sleep.
   * This method also calls the averageThrottler inside
   * @param deltaProgress         Progress since the last limit call
   */
  virtual double calculateSleep(double bytesTotalProgress,
                                const Clock::time_point& now);

  /// Provides anyone using this throttler instance to update the throttler
  /// rates
  void setThrottlerRates(double avgRateBytesPerSec,
                         double bucketRateBytesPerSec,
                         double bytesTokenBucketLimit);

  virtual ~Throttler() {
  }

  /// Anyone who is using the throttler should call this method to maintain
  /// the refCount_ and startTime_ correctly
  void registerTransfer();

  /// Method to de-register the transfer and decremenet the refCount_
  void deRegisterTransfer();

  /// Get the average rate in bytes per sec
  double getAvgRateBytesPerSec();

  /// Get the bucket rate in bytes per sec
  double getPeakRateBytesPerSec();

  /// Get the bucket limit in byttes
  double getBucketLimitBytes();

  /// Get the throttler logging time period in millis
  int64_t getThrottlerLogTimeMillis();

  /// Set the throttler logging time in millis
  void setThrottlerLogTimeMillis(int64_t throttlerLogTimeMillis);

  friend std::ostream& operator<<(std::ostream& stream,
                                  const Throttler& throttler);

 private:
  /**
   * This method is invoked repeatedly with the amount of progress made
   * (e.g. number of bytes written) till now. If the total progress
   * till now is over the allowed average progress then it returns the
   * time to sleep for the calling thread
   * @param now                       Pass in the current time stamp
   */
  double averageThrottler(const Clock::time_point& now);

  /**
   * This method periodically prints logs.
   * The period is defined by FLAGS_peak_log_time_ms
   * @params deltaProgress      Progress since last call to limit()
   * @params now                The time point caller has
   * @params sleepTimeSeconds   Duration of sleep caused by limit()
   */
  void printPeriodicLogs(const Clock::time_point& now, double deltaProgress);
  /// Records the time the throttler was started
  Clock::time_point startTime_;

  /**
   * Throttler logs the average and instantaneous progress
   * periodically (check FLAGS_peak_log_time_ms). lastLogTime_ is
   * the last time this log was written
   */
  std::chrono::time_point<std::chrono::high_resolution_clock> lastLogTime_;
  /// Instant progress in the time stats were logged last time
  double instantProgress_;
  // Records the total progress in bytes till now
  double bytesProgress_;
  /// Last time the token bucket was filled
  std::chrono::time_point<std::chrono::high_resolution_clock> lastFillTime_;
  /// Number of tokens in the bucket
  int64_t bytesTokenBucket_;
  /// Controls the access across threads
  folly::SpinLock throttlerMutex_;
  /// Number of users of this throttler
  int64_t refCount_;
  /// The average rate expected in bytes
  double avgRateBytesPerSec_;
  /// Limit on the max number of tokens
  double bytesTokenBucketLimit_;
  /// Rate at which bucket is filled
  double bucketRateBytesPerSec_;
  /// Interval between every print of throttler logs
  int64_t throttlerLogTimeMillis_;
};
}
}  // facebook::wdt
