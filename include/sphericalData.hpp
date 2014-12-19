#pragma once

#include <Eigen/Dense>

template<typename T>
struct Spherical //: public DataSpace<T>
{
  class Cluster
  {
    protected:
    Matrix<T,Dynamic,1> centroid_;
    Matrix<T,Dynamic,1> xSum_;
    uint32_t N_;

    public:

    Cluster() : centroid_(0,1), xSum_(0,1), N_(0)
    {};

    Cluster(const Matrix<T,Dynamic,1>& x_i) : centroid_(x_i), xSum_(x_i), N_(1)
    {};

    Cluster(const Matrix<T,Dynamic,1>& xSum, uint32_t N) :
      centroid_(xSum/xSum.norm()), xSum_(xSum), N_(N)
    {};

    T dist (const Matrix<T,Dynamic,1>& x_i) const
    { return Spherical::dist(this->centroid_, x_i); };

    void computeSS(const Matrix<T,Dynamic,Dynamic>& x,  const VectorXu& z,
        const uint32_t k)
    {
      const uint32_t D = x.rows();
      const uint32_t N = x.cols();
      N_ = 0;
      xSum_.setZero(D);
      for(uint32_t i=0; i<N; ++i)
        if(z(i) == k)
        {
          xSum_ += x.col(i); 
          ++ N_;
        }
      //TODO: cloud try to do sth more random here
      if(N_ == 0)
      {
        xSum_ = Matrix<T,Dynamic,1>::Zero(D);
        xSum_(0) = 1.;
      }
    };

    void updateCenter()
    {
      centroid_ = xSum_/xSum_.norm();
    };

    void computeCenter(const Matrix<T,Dynamic,Dynamic>& x,  const VectorXu& z,
        const uint32_t k)
    {
      computeSS(x,z,k);
      updateCenter();
    };

    bool isInstantiated() const {return this->N_>0;};

    uint32_t N() const {return N_;};
    uint32_t& N(){return N_;};
    const Matrix<T,Dynamic,1>& centroid() const {return centroid_;};
    const Matrix<T,Dynamic,1>& xSum() const {return xSum_;};
  };


  class DependentCluster : public Cluster
  {
    protected:
    // variables
    T t_;
    T w_;
    // parameters
    T beta_;
    T lambda_;
    T Q_;

    public:

    DependentCluster() : Cluster(), t_(0), w_(0), beta_(1), lambda_(1), Q_(1)
    {};

    DependentCluster(const Matrix<T,Dynamic,1>& x_i) : Cluster(x_i), t_(0),
      w_(0), beta_(1), lambda_(1), Q_(1)
    {};

    DependentCluster(const Matrix<T,Dynamic,1>& x_i, T beta, T lambda, T Q) :
      Cluster(x_i), t_(0), w_(0), beta_(beta), lambda_(lambda), Q_(Q)
    {};

    DependentCluster(const Matrix<T,Dynamic,1>& x_i, const DependentCluster& cl0) :
      Cluster(x_i), t_(0), w_(0), beta_(cl0.beta()), lambda_(cl0.lambda()),
      Q_(cl0.Q())
    {};

    DependentCluster(T beta, T lambda, T Q) :
      Cluster(), t_(0), w_(0), beta_(beta), lambda_(lambda), Q_(Q)
    {};

    DependentCluster(const DependentCluster& b) :
      Cluster(b.xSum(), b.N()), t_(b.t()), w_(b.w()), beta_(b.beta()),
      lambda_(b.lambda()), Q_(b.Q())
    {};

    DependentCluster* clone(){return new DependentCluster(*this);}

    bool isDead() const {return t_*Q_ < lambda_;};

    void incAge() { ++ t_; };

    void print() const 
    {
      cout<<"cluster " <<"\tN="<<this->N_ <<"\tage="<<t_ <<"\tweight="
        <<w_ <<"\t dead? "<<this->isDead()
        <<"  center: "<<this->centroid().transpose()<<endl;
    };

    void updateWeight()
    {
      T phi, theta, eta;
      T zeta = acos(max(static_cast<T>(-1.),min(static_cast<T>(1.0),
              dist(this->xSum_,this->centroid_)/this->xSum_.norm())));
      Spherical::solveProblem2(this->xSum_ , zeta, t_, w_, beta_, phi,theta,eta);
      w_ = w_ == 0.0? this->xSum_.norm() : w_ * cos(theta) + beta_*t_*cos(phi)
        + this->xSum_.norm()*cos(eta);
      t_ =  0;
    };

    void reInstantiate()
    {
      T phi, theta, eta;
      T zeta = acos(max(static_cast<T>(-1.),min(static_cast<T>(1.0),
              dist(this->xSum_,this->centroid_)/this->xSum_.norm())));
      Spherical::solveProblem2(this->xSum_ , zeta, t_, w_, beta_, phi,theta,eta);

      // rotate point from mean_k towards previous mean by angle eta?
      this->centroid_ = rotationFromAtoB<T>(this->xSum_/this->xSum_.norm(), 
          this->centroid_, eta/(phi*t_+theta+eta)) * this->xSum_/this->xSum_.norm(); 
    };

    void reInstantiate(const Matrix<T,Dynamic,Dynamic>& x_i)
    {
      this->xSum_ = x_i; this->N_ = 1;
      reInstantiate();
    };

    T dist (const Matrix<T,Dynamic,1>& x_i) const
    {
      if(this->isInstantiated())
        return Spherical::dist(this->centroid_, x_i);
      else{
        T phi, theta, eta;
        T zeta = acos(max(static_cast<T>(-1.),min(static_cast<T>(1.0),
                Spherical::dist(x_i,this->centroid_) )));
        Spherical::solveProblem2(x_i, zeta, t_, w_, beta_, phi,theta,eta);

        return w_*(cos(theta)-1.) + t_*beta_*(cos(phi)-1.) + Q_*t_
          + cos(eta); // no minus 1 here cancels with Z(beta) from the two other assignments
      }
    };

    T beta() const {return beta_;};
    T lambda() const {return lambda_;};
    T Q() const {return Q_;};
    T t() const {return t_;};
    T w() const {return w_;};

    uint32_t globalId; // id globally - only increasing id
  };

  static T dist(const Matrix<T,Dynamic,1>& a, const Matrix<T,Dynamic,1>& b)
  { return a.transpose()*b; };

  static T dissimilarity(const Matrix<T,Dynamic,1>& a, const Matrix<T,Dynamic,1>& b)
  { return acos(min(1.0,max(-1.0,(a.transpose()*b)(0)))); };

  static bool closer(const T a, const T b)
  { return a > b; };

  static Matrix<T,Dynamic,Dynamic> computeSums(const
      Matrix<T,Dynamic,Dynamic>& x, const VectorXu& z, const uint32_t K);

  static Matrix<T,Dynamic,1> computeSum(const Matrix<T,Dynamic,Dynamic>& x, 
      const VectorXu& z, const uint32_t k, uint32_t* N_k);

  static Matrix<T,Dynamic,Dynamic> computeCenters(const
      Matrix<T,Dynamic,Dynamic>& x, const VectorXu& z, const uint32_t K, 
      VectorXu& Ns);

  // TODO deprecate soon
  static Matrix<T,Dynamic,1> computeCenter(const Matrix<T,Dynamic,Dynamic>& x, 
      const VectorXu& z, const uint32_t k, uint32_t* N_k);

  static Matrix<T,Dynamic,1> reInstantiatedOldCluster(const
      Matrix<T,Dynamic,1>& xSum, const Matrix<T,Dynamic,1>& ps_k, const T t_k,
      const T w_k, const T beta);

  static T updateWeight(const Matrix<T,Dynamic,1>& xSum, const uint32_t N_k,
      const Matrix<T,Dynamic,1>& ps_k, const T t_k, const T w_k, const T beta);

  static T clusterIsDead(const T t_k, const T lambda, const T Q)
  { return t_k*Q < lambda;};

  static T distToUninstantiated(const Matrix<T,Dynamic,1>& x_i, const
      Matrix<T,Dynamic,1>& ps_k, const T t_k, const T w_k, const T beta,
      const T Q);
  
  private:

  static void solveProblem1(T gamma, T age, const T beta, T& phi, T& theta); 
  static void solveProblem2(const Matrix<T,Dynamic,1>& xSum, T zeta, T age, T w,
      const T beta, T& phi, T& theta, T& eta); 
};

// ================================ impl ======================================

template<typename T>                                                            
T Spherical<T>::distToUninstantiated(const Matrix<T,Dynamic,1>& x_i, const
    Matrix<T,Dynamic,1>& ps_k, const T t_k, const T w_k, const T beta,
    const T Q)
{
  assert(k<this->psPrev_.cols());

  T phi, theta, eta;
  T zeta = acos(max(static_cast<T>(-1.),min(static_cast<T>(1.0),
          dist(x_i,ps_k) )));
  solveProblem2(x_i, zeta, t_k, w_k, beta, phi,theta,eta);

  return w_k*(cos(theta)-1.) 
    + t_k*beta*(cos(phi)-1.) 
    + cos(eta) // no minus 1 here cancels with Z(tau) from the two other assignments
    + Q*t_k;
};

template<typename T>                                                            
Matrix<T,Dynamic,Dynamic> Spherical<T>::computeSums(const
    Matrix<T,Dynamic,Dynamic>& x, const VectorXu& z, const uint32_t K)
{
  const uint32_t D = x.rows();
  Matrix<T,Dynamic,Dynamic> xSums(D,K);
#pragma omp parallel for 
  for(uint32_t k=0; k<K; ++k)
    xSums.col(k) = computeSum(x,z,k,NULL);
  return xSums;
}

  template<typename T>                                                            
Matrix<T,Dynamic,1> Spherical<T>::computeSum(const Matrix<T,Dynamic,Dynamic>& x, 
    const VectorXu& z, const uint32_t k, uint32_t* N_k)
{
  const uint32_t D = x.rows();
  const uint32_t N = x.cols();
  Matrix<T,Dynamic,1> xSum(D);
  xSum.setZero(D);
  if(N_k) *N_k = 0;
  for(uint32_t i=0; i<N; ++i)
    if(z(i) == k)
    {
      xSum += x.col(i); 
      if(N_k) (*N_k) ++;
    }
  return xSum;
};

template<typename T>                                                            
Matrix<T,Dynamic,Dynamic> Spherical<T>::computeCenters(const
    Matrix<T,Dynamic,Dynamic>& x, const VectorXu& z, const uint32_t K, 
    VectorXu& Ns)
{
  const uint32_t D = x.rows();
  Matrix<T,Dynamic,Dynamic> centroids(D,K);
#pragma omp parallel for 
  for(uint32_t k=0; k<K; ++k)
    centroids.col(k) = computeCenter(x,z,k,&Ns(k));
  return centroids;
}

  template<typename T>                                                            
Matrix<T,Dynamic,1> Spherical<T>::computeCenter(const Matrix<T,Dynamic,Dynamic>& x, 
    const VectorXu& z, const uint32_t k, uint32_t* N_k)
{
  const uint32_t D = x.rows();
  Matrix<T,Dynamic,1> mean_k = computeSum(x,z,k,N_k);
  if(*N_k > 0)
    return mean_k/mean_k.norm();
  else
  {
    mean_k = Matrix<T,Dynamic,1>::Zero(D);
    mean_k(0) = 1.;
    return mean_k;
  }
};

template<typename T>                                                            
Matrix<T,Dynamic,1> Spherical<T>::reInstantiatedOldCluster(const
    Matrix<T,Dynamic,1>& xSum, const Matrix<T,Dynamic,1>& ps_k, const T t_k, const
    T w_k, const T beta)
{
  //  cout<<"xSum: "<<xSum.transpose()<<endl;
  T phi, theta, eta;
  T zeta = acos(max(static_cast<T>(-1.),min(static_cast<T>(1.0),
          dist(xSum,ps_k)/xSum.norm())));
  solveProblem2(xSum , zeta, t_k, w_k, beta, phi,theta,eta);

  // rotate point from mean_k towards previous mean by angle eta?
  return rotationFromAtoB<T>(xSum/xSum.norm(), 
      ps_k, eta/(phi*t_k+theta+eta)) * xSum/xSum.norm(); 
};

template<typename T>                                                            
T Spherical<T>::updateWeight(const Matrix<T,Dynamic,1>& xSum, 
    const uint32_t N_k, const Matrix<T,Dynamic,1>& ps_k, const T t_k, 
    const T w_k, const T beta)
{
  //  cout<<"xSum: "<<xSum.transpose()<<endl;
  T phi, theta, eta;
  T zeta = acos(max(static_cast<T>(-1.),min(static_cast<T>(1.0),
          dist(xSum,ps_k)/xSum.norm())));
  solveProblem2(xSum , zeta, t_k, w_k, beta, phi,theta,eta);

  return w_k * cos(theta) + beta*t_k*cos(phi) + xSum.norm()*cos(eta);
};

template<class T>
void Spherical<T>::solveProblem1(T gamma, T age, const T beta, T& phi, T& theta)
{
  // solves
  // (1)  sin(phi) beta = sin(theta)
  // (2)  gamma = T phi + theta
  // for phi and theta
  phi = 0.0; 

  for (uint32_t i=0; i< 10; ++i)
  {
    T sinPhi = sin(phi);
    T f = - gamma + age*phi + asin(beta*sinPhi);
    // mathematica
    T df = age + (beta*cos(phi))/sqrt(1.-beta*beta*sinPhi*sinPhi); 
    T dPhi = f/df;
    phi = phi - dPhi; // Newton iteration
//    cout<<"@i="<<i<<": "<<phi<<"\t"<<dPhi<<endl;
    if(fabs(dPhi) < 1e-6) break;
  }

  theta = asin(beta*sin(phi));
};


template<class T>
void Spherical<T>::solveProblem2(const Matrix<T,Dynamic,1>& xSum, T zeta, 
    T age, T w, const T beta, T& phi, T& theta, T& eta)
{
  // solves
  // w sin(theta) = beta sin(phi) = ||xSum||_2 sin(eta) 
  // eta + T phi + theta = zeta = acos(\mu0^T xSum/||xSum||_2)
  phi = 0.0;

//  cout<<"w="<<w<<" age="<<age<<" zeta="<<zeta<<endl;

  T L2xSum = xSum.norm();
  for (uint32_t i=0; i< 10; ++i)
  {
    T sinPhi = sin(phi);
    T cosPhi = cos(phi);
    T f = - zeta + asin(beta/L2xSum *sinPhi) + age * phi + asin(beta/w *sinPhi);
    T df = age + (beta*cosPhi)/sqrt(L2xSum*L2xSum -
        beta*beta*sinPhi*sinPhi) + (beta*cosPhi)/sqrt(w*w -
        beta*beta*sinPhi*sinPhi); 

    T dPhi = f/df;

    phi = phi - dPhi; // Newton iteration
//    cout<<"@i="<<i<<": "<<"f="<<f<<" df="<<df<<" phi="<<phi<<"\t"<<dPhi<<endl;
    if(fabs(dPhi) < 1e-6) break;
  }

  theta = asin(beta/w *sin(phi));
  eta = asin(beta/L2xSum *sin(phi));
};

