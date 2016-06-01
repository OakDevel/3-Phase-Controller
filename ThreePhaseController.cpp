/* 
 * File:   ThreePhaseController.cpp
 * Author: Cameron
 * 
 * Created on October 22, 2015, 2:21 AM
 */

#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <util/delay.h>

#include "ThreePhaseController.h"
#include "MLX90363.h"
#include "ThreePhaseDriver.h"
#include "Debug.h"
#include "Interpreter.h"
#include "Predictor.h"

bool ThreePhaseController::isForwardTorque;
u1 ThreePhaseController::magRoll;

void TIMER4_OVF_vect() {
 ThreePhaseController::isr();
}

void ThreePhaseController::isr() {
 u1 static mlx = 1;

 // Scale phase to output range
 u2 outputPhase = Predictor::predict();

 // Offset from current angle by 90deg for max torque
 if (isForwardTorque) outputPhase -= ThreePhaseDriver::StepsPerCycle / 4;
 else                 outputPhase += ThreePhaseDriver::StepsPerCycle / 4;
 
 // Fix outputPhase range
 if (outputPhase > ThreePhaseDriver::StepsPerCycle) {
  // Fix it
  if (isForwardTorque) outputPhase += ThreePhaseDriver::StepsPerCycle;
  else                 outputPhase -= ThreePhaseDriver::StepsPerCycle;
 }
 
 // Update driver outputs
 ThreePhaseDriver::advanceTo(outputPhase);
 
 // Don't continue if we're not done counting down
 if (--mlx)
  return;
 
 MLX90363::startTransmitting();
 
 mlx = cyclesPWMPerMLX;
}

//size of table in program memory
u2 constexpr loop = 4098;

/**
 * 12-bit lookup table for magnetometer Alpha value to Phase value
 */
static const u2 AlphaToPhaseLookup[loop] PROGMEM = {
 // Table generated with python on calibration branch
 0x6160,0x615f,0x615f,0x615e,0x615d,0x615b,0x615b,0x615a,0x6159,0x6158,0x6156,0x6155,0x6153,0x6152,0x6150,0x614f,0x614d,0x614b,0x6149,0x6147,
 0x6144,0x6143,0x6141,0x613f,0x613d,0x613b,0x613a,0x6138,0x6136,0x6135,0x6133,0x6131,0x6130,0x612e,0x612c,0x612a,0x6129,0x6128,0x6126,0x6125,
 0x6123,0x6121,0x611f,0x611d,0x611b,0x6119,0x6117,0x6115,0x6113,0x6111,0x610e,0x610c,0x6109,0x6107,0x6105,0x6100,0x60fc,0x60fa,0x60f8,0x60f5,
 0x60f4,0x60f2,0x60f1,0x60f0,0x60ee,0x60ed,0x60ec,0x60ec,0x60ea,0x60e9,0x60e9,0x60e8,0x60e7,0x60e6,0x60e6,0x60e5,0x60e5,0x60e4,0x60e3,0x60e3,
 0x60e2,0x60e1,0x60e0,0x60de,0x60dd,0x60dc,0x60da,0x60d9,0x60d8,0x60d6,0x60d3,0x60d2,0x60cf,0x60cd,0x60cb,0x60c8,0x60c6,0x60c4,0x60c1,0x60bf,
 0x60bd,0x60ba,0x60b8,0x60b5,0x60b3,0x6019,0x6012,0x60ad,0x60aa,0x60a8,0x60a7,0x60a5,0x60a3,0x60a1,0x609f,0x609d,0x609b,0x6099,0x6097,0x6095,
 0x6093,0x6091,0x608e,0x608d,0x608b,0x6089,0x6088,0x6086,0x6084,0x6082,0x607f,0x607d,0x607b,0x6079,0x6077,0x6076,0x6074,0x6072,0x6071,0x6070,
 0x606e,0x606d,0x606b,0x606a,0x6069,0x6069,0x6068,0x6068,0x6068,0x6067,0x6067,0x6067,0x6066,0x6066,0x6065,0x6065,0x6065,0x6064,0x6064,0x6064,
 0x6063,0x6062,0x6061,0x605c,0x6060,0x605f,0x605e,0x605d,0x605b,0x605a,0x6059,0x6057,0x6056,0x6054,0x6052,0x6051,0x604f,0x604d,0x604b,0x6048,
 0x6046,0x6044,0x6042,0x6040,0x603e,0x603c,0x6039,0x6038,0x6035,0x6033,0x6031,0x6030,0x602d,0x602c,0x602a,0x6029,0x6027,0x6026,0x6024,0x6023,
 0x6021,0x6020,0x601e,0x601d,0x601c,0x601a,0x6019,0x6017,0x6016,0x6014,0x6013,0x6012,0x6010,0x600f,0x600d,0x600b,0x600a,0x6008,0x6006,0x6005,
 0x6003,0x6000,0x52fd,0x52fa,0x52fa,0x52f8,0x52f7,0x52f5,0x52f5,0x52f3,0x52f3,0x52f2,0x52f1,0x52f0,0x52f0,0x52f0,0x52ee,0x52ed,0x52ed,0x52ed,
 0x52ed,0x52ed,0x52ec,0x52ec,0x52ec,0x52eb,0x52eb,0x52ea,0x52e9,0x52e9,0x52e8,0x52e8,0x52e7,0x52e6,0x52e6,0x52e5,0x52e4,0x52e4,0x52e3,0x52e2,
 0x52e0,0x52df,0x52dd,0x52dd,0x52db,0x52d9,0x52d8,0x52d7,0x52d5,0x52d3,0x52d2,0x52d0,0x52ce,0x52cd,0x52cb,0x52c8,0x52c7,0x52c5,0x52c4,0x52c2,
 0x52c0,0x52be,0x52bd,0x52bb,0x52ba,0x52b8,0x52b7,0x52b5,0x52b3,0x52b2,0x52b1,0x52b0,0x52ae,0x52ae,0x52ac,0x52aa,0x52aa,0x52a8,0x52a7,0x52a6,
 0x52a6,0x52a5,0x52a3,0x52a2,0x52a1,0x52a1,0x52a0,0x529f,0x529d,0x529c,0x529b,0x529a,0x5299,0x5298,0x5297,0x5295,0x5295,0x5293,0x5291,0x5290,
 0x528f,0x528e,0x528c,0x528b,0x5289,0x5288,0x5287,0x5285,0x5284,0x5282,0x5280,0x527f,0x527d,0x527c,0x527a,0x5279,0x5277,0x5277,0x5276,0x5274,
 0x5273,0x5272,0x5271,0x5270,0x526f,0x526e,0x526d,0x526b,0x526b,0x526a,0x5269,0x5268,0x5267,0x5266,0x5265,0x5265,0x5264,0x5263,0x5262,0x5261,
 0x5260,0x525f,0x525f,0x525d,0x525c,0x525b,0x525a,0x5259,0x5259,0x5258,0x5256,0x5255,0x5254,0x5252,0x5251,0x5250,0x524f,0x524d,0x524c,0x524b,
 0x5249,0x5248,0x5246,0x5244,0x5243,0x5242,0x5240,0x523f,0x523e,0x523c,0x523b,0x523a,0x5239,0x5238,0x5237,0x5235,0x5235,0x5233,0x5232,0x5232,
 0x5231,0x5230,0x522f,0x522e,0x522d,0x522c,0x522c,0x522b,0x522a,0x5229,0x5229,0x5228,0x5228,0x5227,0x5227,0x5226,0x5225,0x5224,0x5223,0x5223,
 0x5222,0x5222,0x5221,0x5220,0x521f,0x521e,0x521d,0x521c,0x521c,0x521b,0x521a,0x5219,0x5218,0x5217,0x5216,0x5214,0x5213,0x5212,0x5211,0x520f,
 0x520d,0x520c,0x520b,0x5209,0x5208,0x5206,0x5205,0x5201,0x51fe,0x51fb,0x51f9,0x51f8,0x51f6,0x51f5,0x51f3,0x51f2,0x51f1,0x51ef,0x51ee,0x51ec,
 0x51eb,0x51ea,0x51e8,0x51e7,0x51e5,0x51e5,0x51e3,0x51e2,0x51e1,0x51e0,0x51df,0x51de,0x51dd,0x51dc,0x51db,0x51da,0x51d9,0x51d8,0x51d8,0x51d7,
 0x51d6,0x51d6,0x51d5,0x51d3,0x51d3,0x51d2,0x51d1,0x51d0,0x51cf,0x51cf,0x51ce,0x51cd,0x51cc,0x51cb,0x51ca,0x51ca,0x51c9,0x51c8,0x51c7,0x51c6,
 0x51c5,0x51c4,0x51c3,0x51c3,0x51c2,0x51c0,0x51bf,0x51bf,0x51bd,0x51bd,0x51bc,0x51bb,0x51ba,0x51b8,0x51b8,0x51b7,0x51b6,0x51b5,0x51b4,0x51b3,
 0x51b3,0x51b2,0x51b2,0x51b1,0x51b0,0x51b0,0x51af,0x51af,0x51ae,0x51ae,0x51ae,0x51ad,0x51ad,0x51ad,0x51ac,0x51ab,0x51ab,0x51ab,0x51aa,0x51aa,
 0x51a9,0x51a9,0x51a9,0x51a9,0x51a9,0x51a8,0x51a8,0x51a7,0x51a7,0x51a6,0x51a6,0x51a6,0x51a5,0x51a4,0x51a3,0x51a2,0x51a1,0x51a1,0x51a0,0x519f,
 0x519d,0x519c,0x519b,0x519a,0x5199,0x5197,0x5196,0x5195,0x5194,0x5192,0x5190,0x518e,0x518d,0x518b,0x518a,0x5188,0x5187,0x5185,0x5184,0x5182,
 0x5181,0x517e,0x517c,0x517c,0x517a,0x5179,0x5177,0x5176,0x5175,0x5174,0x5172,0x5171,0x5170,0x516f,0x516e,0x516c,0x516b,0x516a,0x516a,0x5169,
 0x5168,0x5167,0x5167,0x5166,0x5165,0x5165,0x5164,0x5163,0x5163,0x5162,0x5161,0x5160,0x5160,0x515f,0x515f,0x515d,0x515d,0x515c,0x515b,0x515a,
 0x515a,0x5159,0x5158,0x5158,0x5157,0x5156,0x5155,0x5154,0x5153,0x5152,0x5151,0x5150,0x514f,0x514e,0x514d,0x514c,0x514b,0x514a,0x5149,0x5148,
 0x5146,0x5146,0x5145,0x5144,0x5143,0x5142,0x5141,0x5140,0x5140,0x513e,0x513e,0x513d,0x513d,0x513c,0x513b,0x513b,0x513a,0x513a,0x5139,0x5139,
 0x5138,0x5137,0x5137,0x5136,0x5135,0x5134,0x5133,0x5133,0x5132,0x5131,0x5130,0x5130,0x512f,0x512d,0x512d,0x512b,0x512a,0x5129,0x5128,0x5127,
 0x5126,0x5124,0x5123,0x5122,0x5120,0x511e,0x511d,0x511b,0x5119,0x5118,0x5116,0x5114,0x5112,0x510f,0x510e,0x510c,0x510a,0x5108,0x5106,0x5105,
 0x5100,0x50ff,0x50fa,0x50f9,0x50f8,0x50f6,0x50f5,0x50f3,0x50f2,0x50f1,0x50ef,0x50ee,0x50ec,0x50eb,0x50ea,0x50e9,0x50e8,0x50e7,0x50e5,0x50e5,
 0x50e4,0x50e3,0x50e2,0x50e1,0x50e0,0x50df,0x50df,0x50de,0x50dd,0x50dc,0x50db,0x50da,0x50d9,0x50d9,0x50d8,0x50d7,0x50d6,0x50d5,0x50d3,0x50d3,
 0x50d1,0x50d0,0x50cf,0x50cd,0x50cc,0x50cb,0x50c9,0x50c8,0x50c7,0x50c5,0x50c4,0x50c2,0x50c0,0x50bf,0x50bd,0x50bc,0x50ba,0x50b8,0x50b7,0x50b5,
 0x50b4,0x50b2,0x50b1,0x50b0,0x50ae,0x50ad,0x50ac,0x50aa,0x50a9,0x50a7,0x50a6,0x50a5,0x50a4,0x50a2,0x50a1,0x50a0,0x509e,0x509d,0x509b,0x509a,
 0x5099,0x5098,0x5097,0x5095,0x5094,0x5092,0x5090,0x508e,0x508d,0x508b,0x508a,0x5088,0x5087,0x5084,0x5082,0x5080,0x507e,0x507c,0x507b,0x5079,
 0x5077,0x5075,0x5072,0x5071,0x5070,0x506e,0x506c,0x506b,0x5069,0x5068,0x5067,0x5065,0x5065,0x5064,0x5063,0x5062,0x5061,0x5060,0x5060,0x505f,
 0x505f,0x505e,0x505e,0x505d,0x505c,0x505c,0x505b,0x505b,0x505b,0x505a,0x505a,0x5059,0x5059,0x5058,0x5058,0x5057,0x5057,0x5056,0x5055,0x5054,
 0x5053,0x5052,0x5051,0x5050,0x504e,0x504d,0x504b,0x5049,0x5048,0x5046,0x5044,0x5043,0x5040,0x503e,0x503d,0x503b,0x5039,0x5038,0x5035,0x5034,
 0x5032,0x5030,0x502e,0x502d,0x502b,0x502a,0x5028,0x5027,0x5026,0x5023,0x5022,0x5020,0x501f,0x501d,0x501c,0x501b,0x5019,0x5017,0x5016,0x5014,
 0x5012,0x5011,0x500f,0x500d,0x500c,0x500a,0x5008,0x5007,0x5006,0x5003,0x42ff,0x42fb,0x42fa,0x42f8,0x42f8,0x42f6,0x42f4,0x42f3,0x42f2,0x42f1,
 0x42f0,0x42ef,0x42ef,0x42ef,0x42ef,0x42ef,0x42ef,0x42ef,0x42ef,0x42ee,0x42ee,0x42ef,0x42ee,0x42ed,0x42ed,0x42ee,0x42ee,0x42ed,0x42ed,0x42ed,
 0x42ea,0x42e9,0x42e9,0x42e8,0x42e7,0x42e5,0x42e5,0x42e3,0x42e1,0x42e0,0x42de,0x42dd,0x42da,0x42d8,0x42d6,0x42d4,0x42d2,0x42cf,0x42cd,0x42ca,
 0x42c8,0x42c5,0x42c3,0x42c1,0x42be,0x42bc,0x42ba,0x42b8,0x42b5,0x42b3,0x42b1,0x42b0,0x42ae,0x42ac,0x42aa,0x42a8,0x42a7,0x42a5,0x42a3,0x42a2,
 0x42a0,0x429e,0x429d,0x429b,0x4299,0x4297,0x4295,0x4293,0x4291,0x4290,0x428e,0x428c,0x428a,0x4288,0x4286,0x4284,0x4282,0x4281,0x427f,0x427c,
 0x427c,0x427a,0x4279,0x4278,0x4277,0x4277,0x4275,0x4274,0x4274,0x4273,0x4272,0x4271,0x4271,0x426f,0x426e,0x426d,0x426b,0x426a,0x4269,0x4267,
 0x4266,0x4265,0x4263,0x4261,0x425f,0x425c,0x425a,0x4258,0x4256,0x4253,0x4250,0x424e,0x424c,0x4248,0x4246,0x4243,0x4240,0x423e,0x423b,0x4238,
 0x4236,0x4234,0x4231,0x422f,0x422d,0x422a,0x4229,0x4227,0x4225,0x4223,0x4221,0x421f,0x421d,0x421c,0x421a,0x4219,0x4217,0x4215,0x4213,0x4212,
 0x420f,0x420e,0x420b,0x4209,0x4207,0x4206,0x4201,0x41fe,0x41fa,0x41f8,0x41f5,0x41f3,0x41f1,0x41ef,0x41ed,0x41ea,0x41e9,0x41e6,0x41e5,0x41e3,
 0x41e1,0x41e0,0x41de,0x41dd,0x41db,0x41da,0x41d9,0x41d8,0x41d7,0x41d6,0x41d4,0x41d3,0x41d1,0x41d0,0x41cf,0x41cd,0x41cc,0x41ca,0x41c9,0x41c8,
 0x41c7,0x41c5,0x41c4,0x41c2,0x41c0,0x41be,0x41bc,0x41ba,0x41b8,0x41b6,0x41b4,0x41b2,0x41b0,0x41ae,0x41ad,0x41ac,0x41aa,0x41a9,0x41a8,0x41a7,
 0x41a6,0x41a5,0x41a5,0x41a4,0x41a3,0x41a2,0x41a2,0x41a1,0x41a1,0x419f,0x419f,0x419d,0x419c,0x419c,0x419b,0x419a,0x4199,0x4198,0x4197,0x4195,
 0x4194,0x4192,0x4190,0x418e,0x418c,0x418a,0x4188,0x4186,0x4184,0x4182,0x417f,0x417c,0x417b,0x4179,0x4177,0x4175,0x4173,0x4172,0x4170,0x416f,
 0x416d,0x416b,0x416a,0x4169,0x4168,0x4167,0x4165,0x4165,0x4163,0x4162,0x4161,0x4160,0x415f,0x415e,0x415d,0x415c,0x415a,0x4159,0x4158,0x4156,
 0x4155,0x4154,0x4152,0x4150,0x414f,0x414e,0x414d,0x414b,0x4149,0x4148,0x4146,0x4145,0x4143,0x4143,0x4141,0x4140,0x413f,0x413e,0x413d,0x413c,
 0x413c,0x413b,0x413a,0x413a,0x413a,0x4139,0x4139,0x4138,0x4138,0x4137,0x4137,0x4136,0x4136,0x4135,0x4134,0x4133,0x4132,0x4131,0x4130,0x412f,
 0x412e,0x412d,0x412b,0x412a,0x4128,0x4126,0x4125,0x4123,0x4121,0x411f,0x411d,0x411a,0x4118,0x4115,0x4113,0x4111,0x410f,0x410c,0x410a,0x4107,
 0x4106,0x4101,0x40fe,0x40fa,0x40f8,0x40f7,0x40f5,0x40f3,0x40f1,0x40ef,0x40ee,0x40ec,0x40ea,0x40e9,0x40e7,0x40e6,0x40e5,0x40e4,0x40e3,0x40e1,
 0x40e0,0x40df,0x40dd,0x40dc,0x40db,0x40da,0x40d9,0x40d8,0x40d6,0x40d5,0x40d3,0x40d2,0x40d0,0x40d0,0x40ce,0x40cd,0x40cb,0x40ca,0x40c9,0x40c8,
 0x40c6,0x40c5,0x40c4,0x40c2,0x40c0,0x40bf,0x40bd,0x40bd,0x40bb,0x40ba,0x40b8,0x40b7,0x40b7,0x40b5,0x40b5,0x40b3,0x40b2,0x40b1,0x40b0,0x40b0,
 0x40af,0x40ae,0x40ad,0x40ac,0x40aa,0x40aa,0x40a9,0x40a7,0x40a6,0x40a5,0x40a4,0x40a3,0x40a1,0x40a0,0x409f,0x409d,0x409b,0x409a,0x4099,0x4097,
 0x4095,0x4094,0x4091,0x4090,0x408e,0x408b,0x408a,0x4088,0x4086,0x4084,0x4081,0x407e,0x407d,0x407c,0x4079,0x4077,0x4075,0x4073,0x4072,0x4070,
 0x406e,0x406c,0x406b,0x4069,0x4067,0x4066,0x4065,0x4064,0x4063,0x4061,0x405f,0x405f,0x405e,0x405d,0x405b,0x405a,0x405a,0x4059,0x4058,0x4058,
 0x4057,0x4056,0x4055,0x4054,0x4053,0x4052,0x4052,0x4051,0x4050,0x404f,0x404f,0x404e,0x404d,0x404c,0x404b,0x404a,0x404a,0x4048,0x4048,0x4046,
 0x4045,0x4044,0x4043,0x4042,0x4041,0x4040,0x403e,0x403d,0x403c,0x403b,0x4039,0x4038,0x4038,0x4037,0x4035,0x4034,0x4033,0x4032,0x4031,0x4030,
 0x402f,0x402d,0x402d,0x402c,0x402a,0x402a,0x4029,0x4028,0x4027,0x4026,0x4025,0x4024,0x4023,0x4022,0x4020,0x4020,0x401e,0x401d,0x401c,0x401b,
 0x401a,0x4019,0x4018,0x4016,0x4015,0x4014,0x4013,0x4012,0x4010,0x400f,0x400e,0x400d,0x400b,0x400a,0x4008,0x4007,0x4006,0x4005,0x4002,0x32fe,
 0x32fb,0x32fa,0x32f9,0x32f8,0x32f5,0x32f4,0x32f3,0x32f1,0x32f0,0x32ef,0x32ee,0x32ec,0x32ec,0x32eb,0x32ea,0x32e9,0x32e8,0x32e7,0x32e7,0x32e6,
 0x32e6,0x32e6,0x32e5,0x32e5,0x32e5,0x32e5,0x32e5,0x32e5,0x32e4,0x32e4,0x32e4,0x32e3,0x32e3,0x32e3,0x32e3,0x32e3,0x32e3,0x32e3,0x32e2,0x32e2,
 0x32e2,0x32e2,0x32e1,0x32e0,0x32e0,0x32e0,0x32df,0x32df,0x32df,0x32df,0x32dd,0x32dc,0x32dc,0x32dc,0x32da,0x32da,0x32d9,0x32d8,0x32d8,0x32d7,
 0x32d6,0x32d4,0x32d3,0x32d2,0x32d0,0x32d0,0x32ce,0x32cd,0x32cb,0x32ca,0x32c8,0x32c8,0x32c6,0x32c5,0x32c3,0x32c2,0x32c0,0x32bf,0x32be,0x32bd,
 0x32bb,0x32ba,0x32b8,0x32b7,0x32b6,0x32b5,0x32b3,0x32b2,0x32b1,0x32b0,0x32af,0x32ae,0x32ad,0x32ac,0x32aa,0x32a9,0x32a7,0x32a7,0x32a6,0x32a5,
 0x32a3,0x32a3,0x32a2,0x32a1,0x329f,0x329e,0x329d,0x329b,0x329b,0x3299,0x3298,0x3297,0x3296,0x3295,0x3293,0x3292,0x3290,0x328f,0x328e,0x328d,
 0x328c,0x328b,0x3289,0x3289,0x3287,0x3286,0x3285,0x3284,0x3283,0x3282,0x3281,0x327f,0x327f,0x327d,0x327c,0x327c,0x327b,0x327b,0x327a,0x3279,
 0x3278,0x3278,0x3278,0x3277,0x3277,0x3277,0x3276,0x3276,0x3276,0x3275,0x3275,0x3274,0x3274,0x3274,0x3274,0x3273,0x3273,0x3273,0x3272,0x3272,
 0x3272,0x3271,0x3271,0x3271,0x3271,0x3270,0x3270,0x326f,0x326e,0x326d,0x326d,0x326b,0x326b,0x326a,0x3269,0x3268,0x3267,0x3266,0x3265,0x3264,
 0x3263,0x3261,0x3260,0x325f,0x325d,0x325b,0x325a,0x3259,0x3257,0x3256,0x3254,0x3252,0x3250,0x324f,0x324d,0x324b,0x3249,0x3248,0x3246,0x3244,
 0x3243,0x3241,0x323f,0x323e,0x323c,0x323b,0x3239,0x3237,0x3236,0x3234,0x3233,0x3231,0x3230,0x322e,0x322d,0x322b,0x322a,0x3228,0x3227,0x3226,
 0x3224,0x3223,0x3221,0x3220,0x321e,0x321d,0x321c,0x321b,0x3219,0x3218,0x3217,0x3216,0x3214,0x3213,0x3212,0x3210,0x320f,0x320e,0x320c,0x320b,
 0x3209,0x3208,0x3207,0x3206,0x3205,0x3201,0x31ff,0x31fb,0x31fa,0x31f9,0x31f8,0x31f7,0x31f5,0x31f4,0x31f3,0x31f1,0x31f0,0x31ef,0x31ee,0x31ed,
 0x31ec,0x31eb,0x31ea,0x31ea,0x31e9,0x31e8,0x31e7,0x31e6,0x31e6,0x31e5,0x31e5,0x31e5,0x31e4,0x31e3,0x31e3,0x31e2,0x31e1,0x31e0,0x31e0,0x31df,
 0x31df,0x31de,0x31dd,0x31dc,0x31db,0x31da,0x31d9,0x31d8,0x31d8,0x31d7,0x31d6,0x31d5,0x31d4,0x31d3,0x31d2,0x31d0,0x31cf,0x31ce,0x31cd,0x31cb,
 0x31ca,0x31c8,0x31c7,0x31c5,0x31c3,0x31c2,0x31c0,0x31be,0x31bd,0x31bb,0x31ba,0x31b8,0x31b6,0x31b5,0x31b3,0x31b1,0x31b0,0x31ae,0x31ae,0x31ac,
 0x31aa,0x31a9,0x31a7,0x31a7,0x31a5,0x31a4,0x31a3,0x31a1,0x31a1,0x319f,0x319d,0x319d,0x319b,0x319a,0x3199,0x3198,0x3197,0x3196,0x3195,0x3194,
 0x3192,0x3191,0x3190,0x318f,0x318e,0x318d,0x318b,0x318a,0x3189,0x3188,0x3187,0x3185,0x3184,0x3182,0x3181,0x317f,0x317d,0x317c,0x317b,0x3179,
 0x3178,0x3177,0x3175,0x3174,0x3172,0x3172,0x3170,0x3170,0x316e,0x316d,0x316c,0x316b,0x316a,0x3169,0x3169,0x3168,0x3167,0x3166,0x3165,0x3165,
 0x3164,0x3163,0x3162,0x3161,0x3160,0x315f,0x315f,0x315e,0x315d,0x315b,0x315b,0x315a,0x3159,0x3158,0x3157,0x3156,0x3154,0x3153,0x3152,0x3150,
 0x314f,0x314e,0x314c,0x314b,0x3149,0x3147,0x3146,0x3144,0x3142,0x3140,0x313f,0x313d,0x313c,0x313b,0x3139,0x3138,0x3137,0x3136,0x3135,0x3134,
 0x3133,0x3132,0x3132,0x3131,0x3130,0x312f,0x312f,0x312e,0x312d,0x312d,0x312c,0x312c,0x312b,0x312a,0x3129,0x3129,0x3128,0x3128,0x3126,0x3126,
 0x3125,0x3123,0x3122,0x3120,0x3120,0x311d,0x311c,0x311b,0x3119,0x3117,0x3115,0x3113,0x3111,0x310f,0x310d,0x310b,0x3108,0x3106,0x3102,0x3100,
 0x30fb,0x30fa,0x30f8,0x30f5,0x30f4,0x30f2,0x30f1,0x30ee,0x30ed,0x30eb,0x30ea,0x30e8,0x30e7,0x30e5,0x30e3,0x30e2,0x30e0,0x30df,0x30dd,0x30dc,
 0x30da,0x30d9,0x30d8,0x30d6,0x30d5,0x30d3,0x30d1,0x30d0,0x30ce,0x30cb,0x30ca,0x30c8,0x30c7,0x30c5,0x30c3,0x30c2,0x30c0,0x30bf,0x30bd,0x30bc,
 0x30bb,0x30ba,0x30b8,0x30b7,0x30b6,0x30b5,0x30b4,0x30b3,0x30b2,0x30b2,0x30b1,0x30b0,0x30af,0x30ae,0x30ae,0x30ad,0x30ac,0x30ab,0x30aa,0x30a9,
 0x30a7,0x30a6,0x30a5,0x30a4,0x30a2,0x30a1,0x309f,0x309d,0x309a,0x3098,0x3096,0x3094,0x3091,0x308e,0x308b,0x3089,0x3086,0x3084,0x3081,0x307e,
 0x307b,0x3079,0x3076,0x3074,0x3072,0x306f,0x306d,0x306b,0x3068,0x3067,0x3065,0x3063,0x3061,0x305f,0x305e,0x305c,0x305b,0x3059,0x3058,0x3056,
 0x3055,0x3054,0x3052,0x3051,0x3050,0x304f,0x304d,0x304b,0x304a,0x3049,0x3047,0x3045,0x3044,0x3042,0x3040,0x303f,0x303d,0x303b,0x303a,0x3039,
 0x3038,0x3037,0x3035,0x3034,0x3033,0x3032,0x3031,0x3030,0x302f,0x302d,0x302c,0x302b,0x302a,0x3028,0x3028,0x3026,0x3024,0x3023,0x3020,0x301f,
 0x301d,0x301b,0x3019,0x3017,0x3015,0x3012,0x300f,0x300d,0x300b,0x3008,0x3006,0x3000,0x22fc,0x22f9,0x22f6,0x22f4,0x22f2,0x22ef,0x22ee,0x22ec,
 0x22ea,0x22e8,0x22e7,0x22e5,0x22e5,0x22e3,0x22e2,0x22e0,0x22df,0x22de,0x22dd,0x22dc,0x22db,0x22da,0x22d9,0x22d8,0x22d8,0x22d6,0x22d5,0x22d4,
 0x22d3,0x22d1,0x22d0,0x22cf,0x22cd,0x22cc,0x22ca,0x22c9,0x22c7,0x22c5,0x22c3,0x22c1,0x22bf,0x22bd,0x22bc,0x22ba,0x22b8,0x22b6,0x22b5,0x22b3,
 0x22b1,0x22b0,0x22ae,0x22ac,0x22aa,0x22a9,0x22a7,0x22a5,0x22a4,0x22a2,0x22a0,0x229e,0x229c,0x229a,0x2298,0x2296,0x2294,0x2291,0x228f,0x228d,
 0x228b,0x2288,0x2286,0x2284,0x2281,0x227f,0x227c,0x227b,0x2279,0x2277,0x2275,0x2273,0x2272,0x2271,0x2270,0x226e,0x226d,0x226c,0x226b,0x226a,
 0x226a,0x2269,0x2269,0x2268,0x2268,0x2267,0x2266,0x2266,0x2266,0x2265,0x2265,0x2263,0x2263,0x2261,0x2260,0x225f,0x225d,0x225c,0x225a,0x2259,
 0x2257,0x2255,0x2253,0x2251,0x224f,0x224d,0x224a,0x2248,0x2245,0x2243,0x2241,0x223e,0x223c,0x223a,0x2238,0x2235,0x2233,0x2230,0x222e,0x222c,
 0x222a,0x2228,0x2226,0x2224,0x2223,0x2220,0x221e,0x221d,0x221b,0x2219,0x2217,0x2215,0x2213,0x2211,0x220f,0x220d,0x220b,0x2208,0x2206,0x2205,
 0x2201,0x21fd,0x21fa,0x21f9,0x21f8,0x21f5,0x21f4,0x21f2,0x21f1,0x21f0,0x21ee,0x21ed,0x21ec,0x21ec,0x21eb,0x21ea,0x21e9,0x21e9,0x21e9,0x21e8,
 0x21e7,0x21e7,0x21e6,0x21e6,0x21e6,0x21e5,0x21e5,0x21e4,0x21e4,0x21e3,0x21e2,0x21e2,0x21e1,0x21e0,0x21df,0x21df,0x21de,0x21dd,0x21dc,0x21da,
 0x21d9,0x21d8,0x21d7,0x21d6,0x21d3,0x21d2,0x21d0,0x21ce,0x21cc,0x21cb,0x21c8,0x21c6,0x21c5,0x21c3,0x21c1,0x21bf,0x21bd,0x21bb,0x21b9,0x21b8,
 0x21b5,0x21b3,0x21b2,0x21b0,0x21ae,0x21ac,0x21aa,0x21a9,0x21a7,0x21a6,0x21a4,0x21a2,0x21a1,0x219f,0x219d,0x219c,0x219a,0x2199,0x2198,0x2197,
 0x2195,0x2193,0x2192,0x2190,0x218f,0x218e,0x218c,0x218b,0x2189,0x2188,0x2186,0x2185,0x2184,0x2182,0x2180,0x217f,0x217c,0x217c,0x217a,0x2179,
 0x2178,0x2177,0x2176,0x2175,0x2174,0x2173,0x2173,0x2172,0x2172,0x2172,0x2171,0x2171,0x2170,0x2170,0x216f,0x216e,0x216d,0x216d,0x216d,0x216c,
 0x216b,0x216a,0x216a,0x2169,0x2168,0x2168,0x2167,0x2166,0x2165,0x2164,0x2163,0x2161,0x2160,0x215f,0x215e,0x215d,0x215b,0x215a,0x2159,0x2157,
 0x2156,0x2154,0x2152,0x2150,0x214f,0x214d,0x214b,0x2149,0x2147,0x2145,0x2143,0x2142,0x2140,0x213e,0x213d,0x213b,0x213a,0x2138,0x2137,0x2136,
 0x2135,0x2133,0x2132,0x2130,0x212f,0x212e,0x212d,0x212b,0x212a,0x2129,0x2128,0x2127,0x2126,0x2125,0x2123,0x2123,0x2122,0x2120,0x211f,0x211e,
 0x211d,0x211c,0x211b,0x211a,0x2119,0x2117,0x2116,0x2115,0x2114,0x2112,0x2112,0x2110,0x210f,0x210d,0x210c,0x210b,0x2109,0x2108,0x2106,0x2106,
 0x2105,0x2100,0x20fe,0x20fb,0x20fa,0x20f8,0x20f7,0x20f6,0x20f5,0x20f3,0x20f2,0x20f1,0x20f0,0x20ef,0x20ee,0x20ed,0x20ec,0x20eb,0x20ea,0x20e9,
 0x20e8,0x20e7,0x20e6,0x20e5,0x20e4,0x20e3,0x20e2,0x20e1,0x20e0,0x20df,0x20de,0x20dd,0x20dc,0x20db,0x20da,0x20d9,0x20d8,0x20d7,0x20d6,0x20d5,
 0x20d3,0x20d3,0x20d1,0x20d0,0x20ce,0x20cd,0x20cb,0x20ca,0x20c9,0x20c8,0x20c6,0x20c5,0x20c3,0x20c2,0x20c1,0x20c0,0x20be,0x20bd,0x20bc,0x20ba,
 0x20b9,0x20b8,0x20b7,0x20b5,0x20b4,0x20b3,0x20b2,0x20b0,0x20b0,0x20ae,0x20ad,0x20ac,0x20ac,0x20aa,0x20a9,0x20a8,0x20a8,0x20a7,0x20a6,0x20a6,
 0x20a6,0x20a5,0x20a5,0x20a4,0x20a3,0x20a2,0x20a2,0x20a2,0x20a1,0x20a1,0x20a0,0x209f,0x209f,0x209e,0x209d,0x209c,0x209c,0x209b,0x209b,0x209a,
 0x2099,0x2098,0x2097,0x2096,0x2095,0x2094,0x2092,0x2090,0x208f,0x208e,0x208d,0x208b,0x208a,0x2089,0x2088,0x2087,0x2085,0x2084,0x2082,0x2080,
 0x207e,0x207d,0x207c,0x207a,0x2079,0x2077,0x2075,0x2074,0x2072,0x2071,0x2070,0x206e,0x206d,0x206b,0x206a,0x2069,0x2067,0x2066,0x2065,0x2064,
 0x2063,0x2062,0x2061,0x2060,0x205f,0x205e,0x205d,0x205b,0x205b,0x205a,0x205a,0x2059,0x2058,0x2057,0x2056,0x2055,0x2054,0x2053,0x2052,0x2052,
 0x2051,0x2050,0x204f,0x204f,0x204e,0x204d,0x204c,0x204b,0x204a,0x2049,0x2048,0x2047,0x2046,0x2045,0x2044,0x2043,0x2043,0x2042,0x2041,0x2040,
 0x203f,0x203e,0x203d,0x203c,0x203b,0x203b,0x203a,0x203a,0x2039,0x2038,0x2038,0x2037,0x2037,0x2037,0x2036,0x2036,0x2035,0x2035,0x2034,0x2034,
 0x2033,0x2033,0x2032,0x2032,0x2032,0x2032,0x2032,0x2031,0x2030,0x2030,0x202f,0x202f,0x202f,0x202e,0x202d,0x202d,0x202c,0x202c,0x202b,0x202a,
 0x202a,0x2029,0x2028,0x2028,0x2028,0x2027,0x2026,0x2025,0x2024,0x2023,0x2022,0x2021,0x2020,0x201f,0x201e,0x201d,0x201b,0x201a,0x2019,0x2017,
 0x2016,0x2014,0x2013,0x2011,0x200f,0x200e,0x200d,0x200b,0x2009,0x2008,0x2006,0x2005,0x2002,0x12ff,0x12fb,0x12fa,0x12f9,0x12f8,0x12f7,0x12f5,
 0x12f4,0x12f3,0x12f2,0x12f1,0x12f0,0x12ef,0x12ee,0x12ed,0x12ec,0x12eb,0x12ea,0x12e9,0x12e8,0x12e7,0x12e6,0x12e5,0x12e5,0x12e3,0x12e3,0x12e2,
 0x12e1,0x12e0,0x12df,0x12de,0x12de,0x12dd,0x12dc,0x12db,0x12da,0x12d9,0x12d9,0x12d8,0x12d8,0x12d7,0x12d6,0x12d5,0x12d4,0x12d3,0x12d2,0x12d1,
 0x12d0,0x12cf,0x12ce,0x12cd,0x12cc,0x12cb,0x12ca,0x12c8,0x12c8,0x12c7,0x12c6,0x12c5,0x12c4,0x12c3,0x12c2,0x12c0,0x12c0,0x12bf,0x12be,0x12bd,
 0x12bc,0x12bc,0x12bb,0x12ba,0x12ba,0x12b9,0x12b8,0x12b7,0x12b7,0x12b6,0x12b5,0x12b4,0x12b3,0x12b2,0x12b2,0x12b1,0x12b0,0x12af,0x12ae,0x12ae,
 0x12ad,0x12ac,0x12aa,0x12a9,0x12a8,0x12a7,0x12a6,0x12a5,0x12a3,0x12a2,0x12a1,0x129f,0x129d,0x129c,0x129a,0x1299,0x1297,0x1296,0x1294,0x1292,
 0x1290,0x128e,0x128d,0x128b,0x1289,0x1288,0x1286,0x1284,0x1282,0x1281,0x127e,0x127c,0x127b,0x1279,0x1278,0x1277,0x1275,0x1274,0x1272,0x1271,
 0x1270,0x126e,0x126d,0x126c,0x126b,0x126a,0x1269,0x1268,0x1267,0x1266,0x1265,0x1264,0x1264,0x1263,0x1261,0x1260,0x1260,0x125f,0x125e,0x125d,
 0x125c,0x125b,0x125a,0x1259,0x1258,0x1258,0x1256,0x1256,0x1254,0x1253,0x1252,0x1250,0x1250,0x124e,0x124d,0x124c,0x124a,0x1249,0x1248,0x1246,
 0x1244,0x1243,0x1241,0x1240,0x123e,0x123d,0x123b,0x123a,0x1238,0x1237,0x1236,0x1234,0x1233,0x1231,0x1230,0x122e,0x122d,0x122b,0x122a,0x1229,
 0x1228,0x1227,0x1226,0x1224,0x1223,0x1221,0x1220,0x121e,0x121d,0x121b,0x121a,0x1218,0x1216,0x1215,0x1213,0x1212,0x1210,0x120e,0x120c,0x120b,
 0x1208,0x1207,0x1205,0x1201,0x11fe,0x11fa,0x11f9,0x11f8,0x11f5,0x11f4,0x11f2,0x11f1,0x11ee,0x11ed,0x11eb,0x11ea,0x11e8,0x11e7,0x11e5,0x11e4,
 0x11e3,0x11e2,0x11e0,0x11e0,0x11de,0x11de,0x11dd,0x11dc,0x11dc,0x11dc,0x11db,0x11da,0x11da,0x11da,0x11d9,0x11d9,0x11d9,0x11d8,0x11d8,0x11d8,
 0x11d8,0x11d7,0x11d6,0x11d6,0x11d5,0x11d5,0x11d3,0x11d3,0x11d2,0x11d1,0x11d0,0x11cf,0x11cd,0x11cc,0x11ca,0x11c8,0x11c7,0x11c5,0x11c4,0x11c2,
 0x11c0,0x11bf,0x11bd,0x11bb,0x11ba,0x11b8,0x11b5,0x11b4,0x11b2,0x11b0,0x11ae,0x11ad,0x11ab,0x11a9,0x11a7,0x11a6,0x11a5,0x11a3,0x11a1,0x119f,
 0x119e,0x119c,0x119b,0x1199,0x1198,0x1196,0x1195,0x1193,0x1191,0x1190,0x118e,0x118d,0x118b,0x1189,0x1188,0x1186,0x1184,0x1184,0x1182,0x117f,
 0x117e,0x117c,0x117b,0x1179,0x1178,0x1177,0x1177,0x1176,0x1175,0x1175,0x1174,0x1174,0x1174,0x1174,0x1174,0x1173,0x1173,0x1173,0x1172,0x1172,
 0x1172,0x1171,0x1171,0x1171,0x1170,0x116f,0x116e,0x116e,0x116d,0x116c,0x116b,0x116a,0x1169,0x1169,0x1167,0x1166,0x1165,0x1164,0x1162,0x1161,
 0x115f,0x115d,0x115b,0x1159,0x1157,0x1154,0x1152,0x1150,0x114e,0x114b,0x1149,0x1147,0x1144,0x1143,0x1140,0x113e,0x113b,0x113a,0x1138,0x1136,
 0x1134,0x1131,0x112f,0x112d,0x112b,0x112a,0x1128,0x1126,0x1124,0x1122,0x1120,0x111e,0x111d,0x111b,0x1119,0x1116,0x1114,0x1112,0x1110,0x110e,
 0x110c,0x110a,0x1108,0x1106,0x1105,0x1100,0x10ff,0x10fb,0x10fa,0x10f8,0x10f7,0x10f5,0x10f4,0x10f3,0x10f3,0x10f2,0x10f0,0x10f0,0x10ee,0x10ed,
 0x10ec,0x10eb,0x10ea,0x10e9,0x10e7,0x10e7,0x10e5,0x10e4,0x10e2,0x10e0,0x10de,0x10dc,0x10da,0x10d8,0x10d6,0x10d3,0x10d1,0x10cf,0x10cc,0x10ca,
 0x10c8,0x10c5,0x10c2,0x10c0,0x10bd,0x10bb,0x10b8,0x10b5,0x10b3,0x10b0,0x10ae,0x10ab,0x10a9,0x10a7,0x10a5,0x10a3,0x10a1,0x10a0,0x109e,0x109c,
 0x109a,0x1099,0x1097,0x1095,0x1094,0x1091,0x1090,0x108e,0x108c,0x108a,0x1089,0x1087,0x1084,0x1082,0x107f,0x107d,0x107b,0x1079,0x1077,0x1075,
 0x1072,0x1071,0x106f,0x106c,0x106a,0x1069,0x1067,0x1065,0x1063,0x1061,0x1060,0x105f,0x105d,0x105b,0x105b,0x1059,0x1059,0x1057,0x1056,0x1054,
 0x1053,0x1052,0x1050,0x1050,0x104e,0x104d,0x104b,0x1049,0x1048,0x1046,0x1044,0x1043,0x1040,0x103f,0x103d,0x103b,0x1039,0x1038,0x1036,0x1035,
 0x1033,0x1032,0x1030,0x102f,0x102e,0x102d,0x102b,0x102a,0x1029,0x1028,0x1028,0x1027,0x1026,0x1025,0x1025,0x1024,0x1023,0x1022,0x1022,0x1021,
 0x1020,0x101f,0x101e,0x101d,0x101c,0x101b,0x1019,0x1018,0x1017,0x1015,0x1013,0x1011,0x100f,0x100d,0x100b,0x1008,0x1006,0x1005,0x1000,0x02fb,
 0x02fa,0x02f8,0x02f6,0x02f4,0x02f2,0x02f1,0x02ef,0x02ee,0x02ec,0x02ea,0x02e9,0x02e7,0x02e6,0x02e5,0x02e4,0x02e3,0x02e1,0x02e0,0x02df,0x02dd,
 0x02dc,0x02db,0x02d9,0x02d8,0x02d8,0x02d6,0x02d5,0x02d4,0x02d3,0x02d2,0x02d0,0x02cf,0x02cd,0x02cc,0x02ca,0x02c9,0x02c8,0x02c7,0x02c5,0x02c4,
 0x02c3,0x02c2,0x02c0,0x02bf,0x02be,0x02bd,0x02bd,0x02bc,0x02bc,0x02bb,0x02ba,0x02b9,0x02b9,0x02b8,0x02b7,0x02b7,0x02b6,0x02b5,0x02b4,0x02b3,
 0x02b2,0x02b2,0x02b1,0x02b0,0x02af,0x02ae,0x02ad,0x02ac,0x02aa,0x02a9,0x02a7,0x02a6,0x02a5,0x02a3,0x02a1,0x029f,0x029d,0x029b,0x029a,0x0298,
 0x0295,0x0293,0x0291,0x028e,0x028d,0x028a,0x0288,0x0286,0x0284,0x0281,0x027e,0x027c,0x027b,0x0279,0x0277,0x0275,0x0274,0x0272,0x0271,0x0270,
 0x026e,0x026c,0x026b,0x0269,0x0268,0x0267,0x0265,0x0264,0x0263,0x0261,0x0260,0x025f,0x025d,0x025b,0x025a,0x0259,0x0258,0x0256,0x0255,0x0253,
 0x0252,0x0250,0x024f,0x024d,0x024b,0x024a,0x0248,0x0247,0x0246,0x0244,0x0243,0x0241,0x0240,0x023e,0x023d,0x023c,0x023b,0x023a,0x0238,0x0237,
 0x0236,0x0235,0x0234,0x0233,0x0232,0x0231,0x0230,0x022f,0x022e,0x022d,0x022c,0x022b,0x022a,0x0229,0x0228,0x0228,0x0226,0x0225,0x0223,0x0222,
 0x0220,0x021f,0x021d,0x021c,0x021a,0x0219,0x0216,0x0215,0x0213,0x0211,0x020f,0x020d,0x020b,0x0209,0x0207,0x0206,0x0202,0x0200,0x01fa,0x01f9,
 0x01f8,0x01f5,0x01f3,0x01f1,0x01ef,0x01ee,0x01ec,0x01ea,0x01e8,0x01e6,0x01e5,0x01e3,0x01e2,0x01e0,0x01df,0x01de,0x01dd,0x01dc,0x01da,0x01d9,
 0x01d8,0x01d8,0x01d8,0x01d6,0x01d6,0x01d5,0x01d4,0x01d3,0x01d3,0x01d2,0x01d1,0x01d0,0x01cf,0x01cf,0x01ce,0x01cd,0x01cd,0x01cb,0x01cb,0x01ca,
 0x01c9,0x01c8,0x01c8,0x01c7,0x01c6,0x01c5,0x01c4,0x01c3,0x01c2,0x01c0,0x01bf,0x01be,0x01bd,0x01bc,0x01bb,0x01ba,0x01b8,0x01b7,0x01b6,0x01b5,
 0x01b3,0x01b2,0x01b1,0x01b0,0x01af,0x01ae,0x01ad,0x01ac,0x01ab,0x01aa,0x01a9,0x01a7,0x01a7,0x01a6,0x01a5,0x01a4,0x01a3,0x01a2,0x01a1,0x01a0,
 0x019f,0x019e,0x019d,0x019b,0x019b,0x019a,0x0199,0x0198,0x0197,0x0195,0x0195,0x0193,0x0192,0x0190,0x018f,0x018e,0x018d,0x018b,0x018a,0x0189,
 0x0187,0x0186,0x0184,0x0184,0x0182,0x0180,0x017e,0x017d,0x017c,0x017a,0x0179,0x0178,0x0177,0x0176,0x0175,0x0173,0x0172,0x0171,0x0171,0x0170,
 0x016e,0x016d,0x016d,0x016c,0x016b,0x016a,0x016a,0x016a,0x0169,0x0169,0x0169,0x0169,0x0169,0x0168,0x0168,0x0168,0x0168,0x0167,0x0167,0x0167,
 0x0167,0x0166,0x0166,0x0166,0x0166,0x0166,0x0165,0x0165,0x0165,0x0165,0x0164,0x0164,0x0163,0x0162,0x0162,0x0161,0x0160,0x0160,0x015f,0x015f,
 0x015e,0x015d,0x015d,0x015b,0x015a,0x015a,0x0159,0x0158,0x0156,0x0156,0x0154,0x0152,0x0151,0x0150,0x014f,0x014d,0x014b,0x014a,0x0148,0x0147,
 0x0146,0x0144,0x0143,0x0142,0x0140,0x013f,0x013e,0x013c,0x013b,0x013a,0x0139,0x0138,0x0137,0x0135,0x0135,0x0133,0x0132,0x0130,0x0130,0x012e,
 0x012d,0x012b,0x012a,0x0129,0x0128,0x0128,0x0126,0x0125,0x0123,0x0123,0x0122,0x0120,0x011f,0x011e,0x011d,0x011b,0x011b,0x0119,0x0118,0x0117,
 0x0116,0x0114,0x0114,0x0112,0x0112,0x0110,0x010f,0x010e,0x010d,0x010c,0x010b,0x0109,0x0108,0x0107,0x0106,0x0105,0x0105,0x0100,0x00fe,0x00fb,
 0x00fb,0x00fa,0x00f9,0x00f9,0x00f8,0x00f7,0x00f7,0x00f6,0x00f5,0x00f5,0x00f4,0x00f4,0x00f4,0x00f3,0x00f3,0x00f3,0x00f3,0x00f2,0x00f2,0x00f1,
 0x00f1,0x00f0,0x00f0,0x00f0,0x00ef,0x00ee,0x00ee,0x00ed,0x00ed,0x00ec,0x00ec,0x00ec,0x00eb,0x00ea,0x00e9,0x00e9,0x00e8,0x00e7,0x00e6,0x00e5,
 0x00e4,0x00e3,0x00e3,0x00e1,0x00e0,0x00df,0x00dd,0x00dd,0x00db,0x00da,0x00d8,0x00d8,0x00d6,0x00d5,0x00d3,0x00d2,0x00d0,0x00cf,0x00cd,0x00cb,
 0x00ca,0x00c8,0x00c6,0x00c5,0x00c3,0x00c1,0x00bf,0x00bd,0x00bc,0x00ba,0x00b8,0x00b7,0x00b5,0x00b3,0x00b1,0x00b0,0x00ae,0x00ad,0x00ab,0x00aa,
 0x00a8,0x00a7,0x00a6,0x00a5,0x00a3,0x00a2,0x00a1,0x009f,0x009d,0x009c,0x009b,0x0099,0x0098,0x0097,0x0096,0x0095,0x0093,0x0091,0x0090,0x008e,
 0x008e,0x008c,0x008b,0x0089,0x0088,0x0087,0x0084,0x0084,0x0082,0x0080,0x007e,0x007d,0x007c,0x007a,0x0079,0x0077,0x0077,0x0075,0x0074,0x0072,
 0x0072,0x0071,0x0070,0x006e,0x006d,0x006c,0x006b,0x006a,0x006a,0x0069,0x0069,0x0068,0x0067,0x0066,0x0066,0x0066,0x0065,0x0065,0x0064,0x0064,
 0x0063,0x0063,0x0062,0x0061,0x0060,0x0060,0x005f,0x005f,0x005e,0x005d,0x005c,0x005b,0x005a,0x0059,0x0059,0x0058,0x0057,0x0056,0x0054,0x0053,
 0x0052,0x0050,0x004f,0x004e,0x004c,0x004b,0x0049,0x0048,0x0046,0x0044,0x0043,0x0041,0x0040,0x003e,0x003c,0x003b,0x0039,0x0038,0x0036,0x0035,
 0x0033,0x0032,0x0030,0x002f,0x002d,0x002b,0x002a,0x0029,0x0028,0x0027,0x0026,0x0025,0x0023,0x0022,0x0021,0x0020,0x001f,0x001e,0x001d,0x001c,
 0x001b,0x001a,0x0019,0x0018,0x0017,0x0016,0x0015,0x0014,0x0012,0x0011,0x0010,0x000f,0x000e,0x000d,0x000b,0x000a,0x0008,0x0007,0x0006,0x0005,
 0x0000,0x62fe,0x62fa,0x62fa,0x62f8,0x62f7,0x62f5,0x62f4,0x62f3,0x62f2,0x62f1,0x62ef,0x62ee,0x62ed,0x62ec,0x62eb,0x62ea,0x62e9,0x62e7,0x62e7,
 0x62e6,0x62e5,0x62e5,0x62e4,0x62e3,0x62e2,0x62e2,0x62e0,0x62e0,0x62df,0x62de,0x62dd,0x62dc,0x62dc,0x62da,0x62d9,0x62d8,0x62d8,0x62d7,0x62d6,
 0x62d5,0x62d3,0x62d2,0x62d1,0x62d0,0x62ce,0x62cd,0x62cb,0x62ca,0x62c8,0x62c7,0x62c6,0x62c4,0x62c2,0x62c1,0x62bf,0x62be,0x62bd,0x62bb,0x62ba,
 0x62b8,0x62b7,0x62b6,0x62b5,0x62b4,0x62b3,0x62b2,0x62b1,0x62b0,0x62af,0x62af,0x62ae,0x62ae,0x62ad,0x62ac,0x62ab,0x62aa,0x62a9,0x62a9,0x62a8,
 0x62a7,0x62a6,0x62a6,0x62a5,0x62a4,0x62a3,0x62a2,0x62a1,0x62a0,0x629e,0x629d,0x629b,0x629a,0x6298,0x6296,0x6295,0x6292,0x6290,0x628e,0x628c,
 0x628a,0x6287,0x6285,0x6283,0x6281,0x627f,0x627c,0x627b,0x6279,0x6277,0x6276,0x6275,0x6272,0x6271,0x6270,0x626e,0x626c,0x626a,0x6269,0x6267,
 0x6266,0x6265,0x6263,0x6262,0x6260,0x625f,0x625d,0x625b,0x625a,0x6259,0x6257,0x6256,0x6254,0x6252,0x6251,0x624f,0x624d,0x624c,0x624a,0x6248,
 0x6246,0x6244,0x6243,0x6241,0x6240,0x623e,0x623c,0x623b,0x623a,0x6238,0x6237,0x6236,0x6235,0x6234,0x6233,0x6232,0x6232,0x6231,0x6230,0x622f,
 0x622e,0x622d,0x622c,0x622b,0x622a,0x6229,0x6228,0x6226,0x6225,0x6223,0x6222,0x6220,0x621e,0x621c,0x621a,0x6218,0x6215,0x6213,0x6210,0x620e,
 0x620b,0x6208,0x6206,0x6200,0x61fd,0x61f9,0x61f6,0x61f3,0x61f1,0x61ee,0x61ec,0x61ea,0x61e7,0x61e5,0x61e3,0x61e1,0x61df,0x61de,0x61dd,0x61db,
 0x61d9,0x61d8,0x61d7,0x61d6,0x61d4,0x61d3,0x61d1,0x61d0,0x61cf,0x61cd,0x61cb,0x61ca,0x61c9,0x61c7,0x61c5,0x61c4,0x61c2,0x61c0,0x61bf,0x61bd,
 0x61bb,0x61ba,0x61b8,0x61b7,0x61b5,0x61b3,0x61b2,0x61b1,0x61b0,0x61af,0x61ae,0x61ad,0x61ac,0x61ab,0x61aa,0x61a9,0x61a8,0x61a7,0x61a6,0x61a5,
 0x61a4,0x61a3,0x61a1,0x61a0,0x619e,0x619c,0x619a,0x6199,0x6196,0x6194,0x6191,0x618e,0x618c,0x6189,0x6187,0x6185,0x6181,0x617f,0x617c,0x617a,
 0x6177,0x6175,0x6173,0x6172,0x6170,0x616d,0x616b,0x616a,0x6168,0x6167,0x6166,0x6165,0x6164,0x6163,0x6162,0x6161,0x6160,0x5fff,
};

/**
 * Converts the magnetometer to a phase value
 * @param alpha 14-bit value from magnetometer
 * @return phase value (0 - 0x2ff inclusive)
 */
inline static u2 lookupAlphaToPhase(u2 alpha) {
 // Make sure we're working with a 14-bit number
 alpha &= 0x3fff;
 
 //divide alpha by 4 to get a feasible table size
 alpha >>= 2;

 //if somehow we get a magnetomiter reading larger then in calibration
 //circle around to begining
 if (alpha > loop){
  alpha -= loop;
 }

 // Read the phase number word from the calculated place in the lookup table
 return pgm_read_word(&AlphaToPhaseLookup[alpha]);
}

void ThreePhaseController::init() {
 MLX90363::init();
 ThreePhaseDriver::init();
 ThreePhaseDriver::setAmplitude(0);
 
 MLX90363::prepareGET1Message(MLX90363::MessageType::Alpha);

 // Enable Timer4 Overflow Interrupt
 TIMSK4 = 1 << TOIE4;
 
 magRoll = MLX90363::getRoll();

 // Get two new readings to get started
 while (!MLX90363::hasNewData(magRoll));
 while (!MLX90363::hasNewData(magRoll));
 
 Predictor::init(lookupAlphaToPhase(MLX90363::getAlpha()));
}

void ThreePhaseController::setTorque(const Torque t) {
 ATOMIC_BLOCK(ATOMIC_FORCEON) {
  isForwardTorque = t.forward;
  ThreePhaseDriver::setAmplitude(t.amplitude);
 }
}

bool ThreePhaseController::updateDriver() {
 if (!MLX90363::hasNewData(magRoll)) return false;
 
 // We can always grab the latest Alpha value safely here
 auto const alpha = MLX90363::getAlpha();
 auto const magPha = lookupAlphaToPhase(alpha);
 
 Predictor::freshPhase(magPha);

 return true;
}
