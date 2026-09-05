#include <lxysort>
#include <iostream>
using namespace std;
int main(){
	vector<int> a;
	a.push_back(1);
	a.push_back(0);
	a.push_back(9);
	lxySort(a);
	for(auto x:a)cout<<x<<" ";
	cout<<"ÎÒ³ÉÁË";
}
