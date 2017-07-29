#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 2;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.5;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

  // State dimension
  n_x_ = x_.size();

  // Augmented state dimension
  n_aug_ = n_x_ + 2;

  // Number of sigma points
  // Number of sigma points = 2 * n_aug_ + 1
  n_sig_ = 2 * n_aug_ + 1;

  // Predicted sigma points
  Xsig_pred_ = MatrixXd(n_x_, n_sig_);

  // Spreading parameter for the sigma points
  lambda_ = 3 - n_aug_;

  // weights for the sigma points
  weights_ = VectorXd(n_sig_);

  is_initialized_ = false;

  time_us_ = 0.0;


  // Measurement noise covariance matrices
  R_laser_ = MatrixXd(2, 2);
  R_radar_ = MatrixXd(3, 3);

  R_radar_ << std_radr_*std_radr_, 0, 0,
          0, std_radphi_*std_radphi_, 0,
          0, 0,std_radrd_*std_radrd_;

  R_laser_ << std_laspx_*std_laspx_,0,
          0,std_laspy_*std_laspy_;





}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  if ((meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_) ||
      (meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_)) {

    if (!is_initialized_) {

      x_ << 0, 0, 0, 0, 0;

      P_ << 0.15,    0, 0, 0, 0,
               0, 0.15, 0, 0, 0,
               0,    0, 1, 0, 0,
               0,    0, 0, 1, 0,
               0,    0, 0, 0, 1;

      time_us_ = meas_package.timestamp_;

      if (meas_package.sensor_type_ == MeasurementPackage::LASER){

        x_(0) = meas_package.raw_measurements_(0);
        x_(1) = meas_package.raw_measurements_(1);
      }
      else if (meas_package.sensor_type_== MeasurementPackage::RADAR){

        float rho = meas_package.raw_measurements_(0);
        float phi = meas_package.raw_measurements_(1);
        float rho_dot = meas_package.raw_measurements_(2);

        float vx = rho_dot * cos(phi);
        float vy = rho_dot * sin(phi);
        float m_v = sqrt(vx*vx + vy*vy);

        x_(0) = rho * cos(phi);
        x_(1) = rho * sin(phi);
        x_(2) = m_v;
      }

      if (fabs(x_(0)) < 0.001 and fabs(x_(1)) < 0.001) {
        x_(0) = 0.001;
        x_(1) = 0.001;
      }



      is_initialized_ = true;

      return;

    }

    ///* Prediction

    float dt = (meas_package.timestamp_ - time_us_) / 1000000.0;
    time_us_ = meas_package.timestamp_;

    Prediction(dt);


    ///* Update
    if (meas_package.sensor_type_ == MeasurementPackage::LASER){
      UpdateLidar(meas_package);
    } else if (meas_package.sensor_type_ == MeasurementPackage::RADAR){
      UpdateRadar(meas_package);
    }
  }
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */

  // Generate sigma points

  // Sigma points matrix
  MatrixXd Xsig = MatrixXd (n_x_, n_sig_);

  // sqrt (P)
  MatrixXd L = P_.llt().matrixL();

  // define lambda
  lambda_ = 3 - n_x_;

  // define sigma points matrix
  // first column
  Xsig.col(0) = x_;

  // rest of locations
  for (int i=0; i<n_x_ ; i++){
    Xsig.col(i+1) = x_ + sqrt(lambda_ + n_x_) * L.col(i);
    Xsig.col(i+1+n_x_) = x_ - sqrt(lambda_ +n_x_) * L.col(i);
  }

  // Sigma points augmentation
  //augmented mean vector
  VectorXd x_aug = VectorXd(n_aug_);

  //augmented state covariance
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);

  //define augmented sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ +1);

  // augmented sigma points lambda
  lambda_ = 3 - n_aug_;

  // augmented mean state
  x_aug.head(5) = x_;
  x_aug(5) = 0;
  x_aug(6) = 0;

  // augmented covariance matrix
  P_aug.fill(0.0);
  P_aug.topLeftCorner(5,5) = P_;
  P_aug(5,5) = std_a_ * std_a_;
  P_aug(6,6) = std_yawdd_*std_yawdd_;

  // sqrt(P_aug)
  MatrixXd L_aug = P_aug.llt().matrixL();

  //augmented sigma points
  Xsig_aug.col(0) = x_aug;
  for (int i = 0; i < n_aug_; i++) {
    Xsig_aug.col(i+1) = x_aug + sqrt( lambda_ + n_aug_) * L_aug.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * L_aug.col(i);
  }


  ///* Sigma points prediction

  for (int i = 0; i < 2 * n_aug_ + 1; i++) {

    // predicted values
    double px_p, py_p, v_p, yaw_p, yawd_p;

    // current values
    double p_x  = Xsig_aug(0, i);
    double p_y  = Xsig_aug(1, i);
    double v  = Xsig_aug(2, i);
    double yaw  = Xsig_aug(3, i);
    double yawd = Xsig_aug(4, i);
    double nu_a = Xsig_aug(5, i);
    double nu_yawdd = Xsig_aug(6, i);

    // remove init problems
    if (fabs(yawd) > 0.001){
      px_p = p_x + v / yawd * (sin(yaw + yawd * delta_t) - sin(yaw));
      py_p = p_y + v / yawd * (cos(yaw) - cos(yaw + yawd * delta_t));
    } else {
      px_p = p_x + v * delta_t * cos(yaw);
      py_p = p_y + v * delta_t * sin(yaw);
    }

    v_p = v;
    yaw_p = yaw + yawd * delta_t;
    yawd_p = yawd;

    // noise
    px_p = px_p + 0.5 * nu_a * delta_t*delta_t*cos(yaw);
    py_p = py_p + 0.5 * nu_a * delta_t*delta_t*sin(yaw);
    v_p = v_p + nu_a * delta_t;
    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p = yawd_p + nu_yawdd*delta_t;

    //update prediction matrix
    Xsig_pred_(0, i) = px_p;
    Xsig_pred_(1, i) = py_p;
    Xsig_pred_(2, i) = v_p;
    Xsig_pred_(3, i) = yaw_p;
    Xsig_pred_(4, i) = yawd_p;

  }

  ///* Extract mean & covariance from sigma points

  // weights
  weights_(0)=lambda_ / (lambda_ + n_aug_);
  for (int i = 1; i < 2 * n_aug_ + 1; i++) {
    double weight = 0.5 / (n_aug_ + lambda_);
    weights_(i) = weight;
  }

  // predicted state mean

  x_.fill(0.0);
  for (int i = 0; i < 2 *n_aug_+1; i++) {
    x_ = x_ + weights_(i) * Xsig_pred_.col(i);
  }

  // Predicted covariance matrix
  P_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    //state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    // normalize angles between -2 PI and 2 PI
    while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

    P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
  }

}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */


  // sensor measurement
  VectorXd z = meas_package.raw_measurements_;

  // measurement dimension
  int n_z = 2;

  // sigma points for the lidar measurements
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  // Transform sigma points into the measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    double p_x = Xsig_pred_(0, i);
    double p_y = Xsig_pred_(1, i);

    // measurement model
    Zsig(0,i) = p_x;
    Zsig(1,i) = p_y;
  }



  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);

  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  // measurement covariance
  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {

    VectorXd z_diff = Zsig.col(i) - z_pred;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }


  // measurement covariance noise
  MatrixXd R = MatrixXd(n_z, n_z);
  R << std_laspx_*std_laspx_, 0,
          0, std_laspy_*std_laspy_;

  S = S + R;

  // cross correlation
  MatrixXd Tc = MatrixXd (n_x_, n_z);


  // UKF update
  // calc Tc
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd z_diff = Zsig.col(i) - z_pred;

    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }


  // Kalman gain
  MatrixXd K = Tc * S.inverse();

  VectorXd z_diff = z - z_pred;

  // Lidar NIS
  NIS_laser_ = z_diff.transpose() * S.inverse() * z_diff;

  // Update
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();

  std::cout << "nis-laser" << "\t" << NIS_laser_ << std::endl;


}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */


  // sensor measurement
  VectorXd z = meas_package.raw_measurements_;

  // measurement dimension
  int n_z = 3;

  // sigma points for the lidar measurements
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  // Transform sigma points into the measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    double p_x = Xsig_pred_(0, i);
    double p_y = Xsig_pred_(1, i);
    double v = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);
    double vx = v * cos(yaw);
    double vy = v * sin(yaw);

    // measurement model
    Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);      // rho
    Zsig(1,i) = atan2(p_y, p_x);      // phi
    Zsig(2,i) =  (p_x*vx + p_y*vy) / sqrt(p_x*p_x + p_y*p_y);   // rho_dot
  }


  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);

  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  // measurement covariance
  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {

    VectorXd z_diff = Zsig.col(i) - z_pred;

    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }


  // measurement covariance noise
  MatrixXd R = MatrixXd(n_z, n_z);
  R << std_radr_*std_radr_, 0, 0,
          0, std_radphi_*std_radphi_, 0,
          0,  0,  std_radrd_*std_radrd_;

  S = S + R;

  // cross correlation
  MatrixXd Tc = MatrixXd (n_x_, n_z);


  // UKF update
  // calc Tc
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;


    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }


  // Kalman gain
  MatrixXd K = Tc * S.inverse();

  VectorXd z_diff = z - z_pred;

  //angle normalization
  while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

  // radar NIS
  NIS_radar_ = z_diff.transpose() * S.inverse() * z_diff;

  // Update
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();

  std::cout << "nis-radar" << "\t" << NIS_radar_ << std::endl;

}
