/**************************************************************************
******Find out the roots & weights of Gauss-Legendre Quadrature for given n 
***************************************************************************/
#include <iostream>
#include <cmath>
#include <list>
#include <vector>


/*Legendre Polynomial P0(x)*/
double P0(double x){
    return 1;
}

/*Legendre Polynomial P1(x)*/
double P1(double x){
    return x;
}

/*Nth Legendre Polynomial Pn(x)*/
double Pn(int n, double x){
    if(n==0) {
        return P0(x);
    }else if(n==1) {
        return P1(x);
    }else{
        //Use the recurrence relation
        return (double )((2*n-1)*x*Pn(n-1,x)-(n-1)*Pn(n-2,x))/n;
    }
}

/*Lagrange terms*/
double Li(int n, double x[n], int i, double X){
    int j;
    double prod=1;
    for(j=0;j<=n;j++){
        if (j!=i){
            prod=prod*(X-x[j])/(x[i]-x[j]);     
        }
    }
    return prod;
}

/*Function definition to perform integration by Simpson's 1/3rd Rule */
double Ci(int i, int n, double x[n], double a, double b, int N){
  double h,integral,X,sum=0;
  int j,k;
  h=(b-a)/N;
  for(j=1;j<N;j++){
    X=a+j*h;
    if(j%2==0){
      sum=sum+2*Li(n-1,x,i,X);
    }
    else{
      sum=sum+4*Li(n-1,x,i,X);;
    }
  }
    double Fa=Li(n-1,x,i,a);;
    double Fb=Li(n-1,x,i,b);
     
 
  integral=(h/3.0)*(Fa+Fb+sum);
  return integral;
}

/*Function definition for bisection procedure[Returns the root if found or 999 for failure]*/
double Bisection(int n,double f(int n,double x),double a, double b, double eps, int maxSteps){
  double c;
  if(f(n,a)*f(n,b)<=0){  
    int iter=1;
    /*Bisection Method begins that tabulates the various values at each iteration*/
    do{
      c=(a+b)/2;
      if(f(n,a)*f(n,c)>0){
      a=c;
    }
    else if(f(n,a)*f(n,c)<0){
      b=c;
    }
    else if(f(n,c)==0){
        return c;
    }
      iter++;
           
    }while(fabs(a-b)>=eps&&iter<=maxSteps);
    return c;
  }
  else{
    return 999;
  }
}
/*
std::vector<std::vector<Real>> RootsAndWeights(int n) {

  //Array to store the roots and weights of Legendre polynomials
  std::vector<std::vector<Real>> roots_weights(2, std::vector<Real>(n));

  //window(Step-size) for bisection method
  double h=0.01;
  //dummy variable for bisection method
  double x;
  //dummy variable where the root is returned after bisection routine
  double root;
  int i=0;

  for(x=-1.0;x<=1.0;x=x+h){
      //set the accuracy to approx. 10^-15 but there is also a limit on maxSteps. (Modify these acc. to your needs)
      root=Bisection(n,Pn,x,x+h,0.0000000000000001,1000000);
      std::cout << "here" << std::endl;

      if(root!=999){
          roots_weights[0][i]=root;
          i++;
      }
  }

  for(i=0;i<n;i++){
      //(Modify the number of sub-intervals according to your needs)
      roots_weights[1][i]=Ci(i,n,roots_weights[0],-1,1,1000000);
  }
  return roots_weights;
}
*/

int main(){
    int i=0;
    int n;      // order/terms 
    printf("Enter the value of n(data-points):\n");
    scanf("%d",&n);
    //Array to store the roots of Legendre polynomials
    double xi[n];
    //window(Step-size) for bisection method
    double h=0.01;
    //dummy variable for bisection method
    double x;
    //dummy variable where the root is returned after bisection routine
    double root;
    printf("\n\nThe roots (xi's) are:\n_____________________________________________________\nAccuracy: 10^(-15)\n\n");
    for(x=-1.0;x<=1.0;x=x+h){
        //set the accuracy to approx. 10^-15 but there is also a limit on maxSteps. (Modify these acc. to your needs)
        root=Bisection(n,Pn,x,x+h,0.00000000001,1000000); 
        if(root!=999){
            xi[i]=root;
            printf("x[%d] = %17.16lf\n",i+1,root);
            i++;
        }
    }
    printf("_____________________________________________________\n");
    printf("\n\nThe weights (ci's) are:\n_____________________________________________________\nAccuracy: 10^(-13)\n\n");
    for(i=0;i<n;i++){
        //(Modify the number of sub-intervals according to your needs)
        printf("c[%d] = %17.16lf\n",i+1,Ci(i,n,xi,-1,1,1000000));
    }
    return 0;
}
